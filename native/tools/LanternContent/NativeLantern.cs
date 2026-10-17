using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Extends the already-built Lantern Lab content pack without rebuilding or
// replacing its proven oil, flame, model, or lighting rows.
static class NativeLantern
{
    const int Torch=60420000, Lantern=60425001;
    static readonly FieldInfo Offset=typeof(PARAM.Row).GetField("DataOffset",BindingFlags.Instance|BindingFlags.NonPublic)!;
    static readonly HashSet<string> SupportedLabRegulations=new(StringComparer.OrdinalIgnoreCase) {
        "77809D67C88FC3D051E5D4218100FB5F004EB52A3B15EA12C37FF5BD5B86BA03",
        "6625A0B846E5EF17C5149F358F4F28676E7A1027EC21434F8EFBB03D30E3FF7E",
        "8DEE71796972C682832CAE4F9749C621451174C4F94BE003E42D08EDA29FF92B",
        "B2D601818725143D6EEE0BC4291195ACC142CADDE701394FBCCB55D97CA0DF64"
    };
    static void Require(bool ok,string message) {if(!ok) throw new InvalidDataException(message);}
    static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes));
    static void GuardOutput(string path) => Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\",StringComparison.OrdinalIgnoreCase),"Output outside research workspace");

    public static void Build(string input,string schemaDirectory,string output,bool testStart,bool testPrecreation)
    {
        GuardOutput(output); Require(!File.Exists(output),"Output already exists; use a new fixture filename");
        var baseline=File.ReadAllBytes(input); Require(SupportedLabRegulations.Contains(Hash(baseline)),"Unsupported Lantern Lab regulation hash");
        var binder=BND4.Read(baseline); var before=binder.Files.ToDictionary(e=>e.Name,e=>e.Bytes.ToArray());
        var defs=Directory.GetFiles(schemaDirectory,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(d=>d.ParamType);
        BinderFile Entry(string name)=>binder.Files.Single(f=>Path.GetFileName(f.Name.Replace('\\','/'))==name);
        PARAM Decode(string name) {var p=PARAM.Read(Entry(name).Bytes);p.ApplyParamdef(defs[p.ParamType]);return p;}
        void Save(string name,PARAM p)=>Entry(name).Bytes=p.Write();
        var item=Decode("ItemParam.param"); Require(item.Rows.All(r=>r.ID!=Lantern),"Wayfarer's Lantern already exists");
        var source=item.Rows.Single(r=>r.ID==Torch); var lantern=new PARAM.Row(Lantern,source.Name,defs[item.ParamType]);
        for(int i=0;i<source.Cells.Count;i++) lantern.Cells[i].Value=source.Cells[i].Value is byte[] b?b.ToArray():source.Cells[i].Value;
        lantern["sortId"].Value=302; lantern["price"].Value=0; lantern["salePrice"].Value=0;
        item.Rows.Add(lantern); item.Rows=item.Rows.OrderBy(r=>r.ID).ToList(); Save("ItemParam.param",item);

        var start=Decode("PlayerStatusParam.param"); var changes=new List<object>();
        foreach(var id in new[]{10,100}.Where(id=>id==10?testPrecreation:testStart)) {
            var row=start.Rows.Single(r=>r.ID==id); var original=row.Cells.Select(c=>c.Value is byte[] b?b.ToArray():c.Value).ToArray();
            var slot=Enumerable.Range(1,10).FirstOrDefault(n=>n!=2&&Convert.ToInt32(row[$"beltItem{n:D2}"].Value)==-1&&Convert.ToInt32(row[$"beltItem{n:D2}Num"].Value)==0);
            Require(slot!=0,$"Test row{id} has no empty quick slot for the Lantern");
            row[$"beltItem{slot:D2}"].Value=Lantern; row[$"beltItem{slot:D2}Num"].Value=(ushort)1;
            var allowed=new HashSet<string>{$"beltItem{slot:D2}",$"beltItem{slot:D2}Num"};
            for(int n=0;n<row.Cells.Count;n++) if(!allowed.Contains(row.Cells[n].Def.InternalName))
                Require(original[n] is byte[] bytes?bytes.SequenceEqual((byte[])row.Cells[n].Value):Equals(original[n],row.Cells[n].Value),$"Unrelated test row{id} field changed");
            changes.Add(new {Row=id,QuickSlot=slot,LanternItem=Lantern,OnlyAllowedFields=allowed.OrderBy(x=>x).ToArray(),OtherFieldsVerified=row.Cells.Count-allowed.Count});
        }
        if(changes.Count>0) Save("PlayerStatusParam.param",start);

        var now=Entry("ItemParam.param").Bytes; var report=new {
            Status="RESEARCH FIXTURE ONLY; native torch-class Lantern candidate. No input/action executor or deployment.",
            InputSHA256=Hash(baseline),OutputSHA256=Hash(binder.Write()),LanternItem=Lantern,TorchSource=Torch,
            TestStart=testStart,TestPrecreation=testPrecreation,TestFixtureChanges=changes,
            ChangedEntries=changes.Count>0?new[]{"ItemParam.param","PlayerStatusParam.param"}:new[]{"ItemParam.param"},
            UntouchedEntries=binder.Files.Count-(changes.Count>0?2:1)
        };
        File.WriteAllBytes(output,binder.Write());
        var verified=BND4.Read(File.ReadAllBytes(output));
        var verifyItem=PARAM.Read(verified.Files.Single(f=>Path.GetFileName(f.Name.Replace('\\','/'))=="ItemParam.param").Bytes); verifyItem.ApplyParamdef(defs[verifyItem.ParamType]);
        Require(verifyItem.Rows.Any(r=>r.ID==Lantern),"Lantern row missing after round trip");
        foreach(var entry in before) if(entry.Key!=Entry("ItemParam.param").Name && (changes.Count==0||entry.Key!=Entry("PlayerStatusParam.param").Name))
            Require(entry.Value.SequenceEqual(verified.Files.Single(f=>f.Name==entry.Key).Bytes),"Unexpected unrelated entry difference: "+entry.Key);
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine("Native Wayfarer's Lantern candidate built and re-read; existing Lab oil/flame/model assets are untouched.");
    }

    public static void Text(string inputDirectory,string outputDirectory)
    {
        GuardOutput(outputDirectory); var texts=new Dictionary<string,string> {
            ["itemname.fmg"]="Wayfarer's Lantern",
            ["simpleexplanation.fmg"]="A reusable lantern. Its fuel reserve is shown by the native torch meter.",
            ["detailedexplanation.fmg"]="A brass lantern carried by a prepared wayfarer.\n\nUse it from the quick slot to draw or stow the lantern.\nLantern Oil replenishes its fuel reserve."
        };
        foreach(var pair in texts) {
            var input=Path.Combine(inputDirectory,pair.Key);var output=Path.Combine(outputDirectory,pair.Key);Require(!File.Exists(output),"FMG output already exists");
            var original=File.ReadAllBytes(input);var f=FMG.Read(original);Require(f.Entries.Any(e=>e.ID==Torch),"Torch item FMG missing");Require(f.Entries.All(e=>e.ID!=Lantern),"Lantern FMG already exists");
            var before=f.Entries.Select(e=>(e.ID,e.Text)).ToArray();f.Entries.Add(new FMG.Entry(Lantern,pair.Value));f.Entries=f.Entries.OrderBy(e=>e.ID).ToList();Directory.CreateDirectory(outputDirectory);f.Write(output);
            var reread=FMG.Read(output);Require(reread.Entries.Count==before.Length+1&&reread.Entries.Single(e=>e.ID==Lantern).Text==pair.Value,"Lantern FMG round-trip failed");
            foreach(var old in before)Require(reread.Entries.Single(e=>e.ID==old.ID).Text==old.Text,"Existing text changed");
        }
        Console.WriteLine("Native Wayfarer's Lantern text added and all existing Lab text preserved.");
    }
}
