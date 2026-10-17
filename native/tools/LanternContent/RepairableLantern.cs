using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Converts the temporary timed-oil Lab candidate into a repair-powder-style
// Oil item. It deliberately does not install an item-use executor: content
// alone must never synthesize a lantern activation or modify live save data.
static class RepairableLantern
{
    const int RepairPowder=60410000, Oil=60425000, Lantern=60425001;
    static readonly HashSet<string> SupportedLanternCandidates=new(StringComparer.OrdinalIgnoreCase) {
        "192C396CA4D8075832E8EB2C69F2ED5AA716F0261CDE549A90F8120FE5D7C14A",
        "D3883AF549618C95B69244036119D3A5CAEDD1E6F01CD0392E9D5334B24174D1",
        "D126E4C55B8EB48AD281550833D10148308B7F462B190383A48E1C49001FEF1F",
        "7DD8908DD0E9DEA928657DB2BD0151730DCAE93F07165C995E44AD5793DECBCA"
    };
    static void Require(bool ok,string message) {if(!ok)throw new InvalidDataException(message);}
    static string Hash(byte[] value)=>Convert.ToHexString(SHA256.HashData(value));
    static void GuardOutput(string path)=>Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\",StringComparison.OrdinalIgnoreCase),"Output outside research workspace");

    public static void Build(string input,string schemaDirectory,string output)
    {
        GuardOutput(output);Require(!File.Exists(output),"Output already exists; use a new fixture filename");
        var baseline=File.ReadAllBytes(input);Require(SupportedLanternCandidates.Contains(Hash(baseline)),"Unsupported active Lantern candidate hash");
        var binder=BND4.Read(baseline);var before=binder.Files.ToDictionary(file=>file.Name,file=>file.Bytes.ToArray());
        var defs=Directory.GetFiles(schemaDirectory,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def=>def.ParamType);
        BinderFile Entry(string name)=>binder.Files.Single(file=>Path.GetFileName(file.Name.Replace('\\','/'))==name);
        PARAM Decode(string name){var param=PARAM.Read(Entry(name).Bytes);param.ApplyParamdef(defs[param.ParamType]);return param;}

        var item=Decode("ItemParam.param");var powder=item.Rows.Single(row=>row.ID==RepairPowder);var oil=item.Rows.Single(row=>row.ID==Oil);
        Require(item.Rows.Any(row=>row.ID==Lantern),"Wayfarer's Lantern item is absent");
        for(var index=0;index<powder.Cells.Count;index++)oil.Cells[index].Value=powder.Cells[index].Value is byte[] bytes?bytes.ToArray():powder.Cells[index].Value;
        oil["iconId"].Value=Oil;oil["sortId"].Value=301;oil["price"].Value=100;oil["salePrice"].Value=10;
        Require(Convert.ToInt32(oil["specialEffectId"].Value)==RepairPowder && Convert.ToInt32(oil["itemTypeParamId"].Value)==700 && Convert.ToInt32(oil["itemUsageParamId"].Value)==1700,"Repair-Powder route did not transfer");
        Entry("ItemParam.param").Bytes=item.Write();

        var active=EMEVD.Read(Entry("SpEffectActiveItem.emevd").Bytes);var timed=active.Events.SingleOrDefault(evt=>evt.ID==Oil);
        Require(timed is not null,"Timed Lantern Oil event is absent");
        active.Events.Remove(timed!);Entry("SpEffectActiveItem.emevd").Bytes=active.Write();

        var changed=binder.Files.Where(file=>!before[file.Name].SequenceEqual(file.Bytes)).Select(file=>file.Name).ToArray();
        Require(changed.Length==2 && changed.Any(name=>Path.GetFileName(name.Replace('\\','/'))=="ItemParam.param") && changed.Any(name=>Path.GetFileName(name.Replace('\\','/'))=="SpEffectActiveItem.emevd"),"Unexpected archive changes");
        foreach(var file in binder.Files.Where(file=>!changed.Contains(file.Name)))Require(before[file.Name].SequenceEqual(file.Bytes),"Unexpected unrelated entry difference: "+file.Name);
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);binder.Write(output);
        var reread=BND4.Read(output);Require(reread.Files.Count==binder.Files.Count,"Archive entry count changed");
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(new {
            Status="RESEARCH CANDIDATE ONLY; Oil now follows the native Repair Powder route. The quick-slot Lantern activation executor and per-lantern service meter remain unimplemented.",
            InputSHA256=Hash(baseline),OutputSHA256=Hash(File.ReadAllBytes(output)),OilItem=Oil,LanternItem=Lantern,RepairPowderSource=RepairPowder,
            OilRoute=new {SpecialEffect=RepairPowder,ItemType=700,ItemUsage=1700,Price=100,SalePrice=10,CustomIcon=Oil},
            RemovedTimedOilEvent=Oil,ChangedEntries=changed,UntouchedEntries=binder.Files.Count-changed.Length,Deployed=false
        },new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine("Repairable-lantern content candidate built and re-read; old timed Oil event removed.");
    }

    public static void Text(string inputDirectory,string outputDirectory)
    {
        GuardOutput(outputDirectory);var replacements=new Dictionary<int,string> {
            [Oil]="Restores the Wayfarer's Lantern's service reserve.",
            [Lantern]="A reusable hip lantern. It never occupies either arm slot."
        };
        foreach(var fileName in new[]{"simpleexplanation.fmg","detailedexplanation.fmg"}) {
            var input=Path.Combine(inputDirectory,fileName);var output=Path.Combine(outputDirectory,fileName);Require(!File.Exists(output),"FMG output already exists");
            var fmg=FMG.Read(input);var before=fmg.Entries.Select(entry=>(entry.ID,entry.Text)).ToArray();
            foreach(var replacement in replacements) {var entry=fmg.Entries.SingleOrDefault(item=>item.ID==replacement.Key);Require(entry is not null,"Expected Lantern Lab text is absent");entry!.Text=fileName=="detailedexplanation.fmg" && replacement.Key==Oil?"A clear lamp oil blend.\n\nUse to service the Wayfarer's Lantern.\nIt follows the same native repair route as Repair Powder.":replacement.Value;}
            Directory.CreateDirectory(outputDirectory);fmg.Write(output);var reread=FMG.Read(output);
            foreach(var old in before.Where(old=>!replacements.ContainsKey(old.ID)))Require(reread.Entries.Single(entry=>entry.ID==old.ID).Text==old.Text,"Existing text changed");
            Require(reread.Entries.Single(entry=>entry.ID==Oil).Text.Contains("service",StringComparison.OrdinalIgnoreCase),"Oil text replacement missing");
            Require(reread.Entries.Single(entry=>entry.ID==Lantern).Text.Contains("arm slot",StringComparison.OrdinalIgnoreCase),"Lantern text replacement missing");
        }
        Console.WriteLine("Repairable-lantern text candidate built; arm-slot-free utility role stated explicitly.");
    }
}
