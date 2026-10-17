using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Offline candidate compiler. The only retained Oil record is an inert save
// compatibility row; never deletes an existing character's inventory.
static class RetireOilVariants
{
    const int Oil=60425000,Lantern=60425001;
    static readonly string[] Inputs={
        "59475AEAFFCF4C7F28099D753A872AF351049EC1964B75D2107E9083C5BF1232",
        "5732DCE25CA7A829387874FB4A5E11911DE7DD346A405BE482E7339E06721347",
        "E66F179C8C7719A540F75934B4EDF597A7346361C80E7D9BB9642E59D8D545B7",
        "DB401571480FDEFC869391B0A8AE01DD6857A397DA5E8F529E1B1FCAB387B771"};
    static readonly string[] Paths={"assets/enc_regulation.bnd.dcx","variants/standard-jump.bnd.dcx","variants/estus-off-jump-on.bnd.dcx","variants/estus-off-jump-off.bnd.dcx"};
    static void Require(bool ok,string message){if(!ok)throw new InvalidDataException(message);}
    static string Hash(byte[] data)=>Convert.ToHexString(SHA256.HashData(data));
    static string Name(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
    static string Cell(object value)=>value is byte[] data?Convert.ToHexString(data):Convert.ToString(value,System.Globalization.CultureInfo.InvariantCulture)!;
    static void Set(PARAM.Row row,string field,object value)=>row[field].Value=Convert.ChangeType(value,row[field].Value.GetType());
    static void VerifyToggle(BND4 b,Dictionary<string,PARAMDEF> defs)
    {
        var items=PARAM.Read(b.Files.Single(f=>Name(f)=="ItemParam.param").Bytes);items.ApplyParamdef(defs[items.ParamType]);
        var row=items.Rows.Single(r=>r.ID==Lantern);
        Require(Convert.ToInt32(row["itemTypeParamId"].Value)==260&&Convert.ToInt32(row["itemUsageParamId"].Value)==2200&&Convert.ToInt32(row["maxNum"].Value)==1&&Convert.ToInt32(row["specialEffectId"].Value)==Lantern,"Reusable Lantern contract changed");
        var evt=EMEVD.Read(b.Files.Single(f=>Name(f)=="SpEffectActiveItem.emevd").Bytes).Events.Single(e=>e.ID==Lantern);
        Require(evt.Instructions.Count==1&&evt.Instructions[0].Bank==1000&&evt.Instructions[0].ID==9&&Convert.ToHexString(evt.Instructions[0].ArgData)=="130000000000000033000000","Owned toggle event changed");
    }
    static object Regulation(string input,string output,string expected,Dictionary<string,PARAMDEF> defs)
    {
        var source=File.ReadAllBytes(input);Require(Hash(source)==expected,"Unexpected input regulation: "+input);
        var b=BND4.Read(source);VerifyToggle(b,defs);
        var original=b.Files.ToDictionary(f=>f.Name,f=>f.Bytes.ToArray());
        var changes=new List<object>();
        foreach(var entry in b.Files.Where(f=>Name(f)=="ItemParam.param"||Name(f)=="PlayerStatusParam.param"||Name(f)=="ShopLineupParam.param"||Name(f).StartsWith("ItemLotParam2_")).ToArray())
        {
            var p=PARAM.Read(entry.Bytes);p.ApplyParamdef(defs[p.ParamType]);
            var before=p.Rows.ToDictionary(r=>r.ID,r=>r.Cells.ToDictionary(c=>c.Def.InternalName,c=>Cell(c.Value)));
            var allowed=new Dictionary<int,HashSet<string>>();var removed=new HashSet<int>();
            foreach(var row in p.Rows.ToArray())
            {
                var fields=new HashSet<string>();
                if(Name(entry)=="ItemParam.param"&&row.ID==Oil)
                {
                    foreach(var field in new[]{"specialEffectId","price","salePrice"}){Set(row,field,0);fields.Add(field);}
                    foreach(var field in new[]{"iconId","startSfxId","actionSfxId","effectSfxId"}){Set(row,field,-1);fields.Add(field);}
                }
                else if(Name(entry)=="PlayerStatusParam.param")
                {
                    for(int n=1;n<=10;n++)if(Convert.ToInt32(row[$"beltItem{n:D2}"].Value)==Oil)
                    {Set(row,$"beltItem{n:D2}",-1);Set(row,$"beltItem{n:D2}Num",0);fields.UnionWith(new[]{$"beltItem{n:D2}",$"beltItem{n:D2}Num"});}
                }
                else if(Name(entry)=="ShopLineupParam.param"&&Convert.ToInt32(row["saleItem"].Value)==Oil)
                {p.Rows.Remove(row);removed.Add(row.ID);continue;}
                else if(Name(entry).StartsWith("ItemLotParam2_"))
                {
                    for(int n=1;n<=10;n++)if(Convert.ToInt64(row[$"itemId{n}"].Value)==Oil)
                    {foreach(var field in new[]{$"itemId{n}",$"itemNum{n}",$"itemWeight{n}"}){Set(row,field,0);fields.Add(field);}}
                }
                if(fields.Count>0)allowed[row.ID]=fields;
            }
            foreach(var row in p.Rows)foreach(var c in row.Cells)
                Require(before[row.ID][c.Def.InternalName]==Cell(c.Value)||(allowed.TryGetValue(row.ID,out var set)&&set.Contains(c.Def.InternalName)),"Unexpected row/field change "+Name(entry)+"/"+row.ID+"/"+c.Def.InternalName);
            if(allowed.Count==0&&removed.Count==0)continue;
            entry.Bytes=p.Write();var verify=PARAM.Read(entry.Bytes);verify.ApplyParamdef(defs[verify.ParamType]);
            Require(verify.Rows.Count==before.Count-removed.Count,"Unexpected row count");
            foreach(var row in p.Rows)
                Require(row.Cells.Select(c=>Cell(c.Value)).SequenceEqual(verify.Rows.Single(r=>r.ID==row.ID).Cells.Select(c=>Cell(c.Value))),"PARAM roundtrip failed");
            changes.Add(new{Entry=Name(entry),Rows=allowed.ToDictionary(a=>a.Key,a=>a.Value.ToArray()),RemovedOilShopRows=removed.ToArray()});
        }
        var activeEntry=b.Files.Single(f=>Name(f)=="SpEffectActiveItem.emevd");var active=EMEVD.Read(activeEntry.Bytes);
        var originalEvents=active.Events.Where(e=>e.ID!=Oil).ToDictionary(e=>e.ID,e=>JsonSerializer.Serialize(e));
        var oilEvent=active.Events.SingleOrDefault(e=>e.ID==Oil);
        if(oilEvent!=null){active.Events.Remove(oilEvent);activeEntry.Bytes=active.Write();changes.Add(new{Entry=Name(activeEntry),RemovedOilEvent=Oil});}
        b.Write(output);var reread=BND4.Read(output);VerifyToggle(reread,defs);
        Require(reread.Files.Count==b.Files.Count,"Binder entry count changed");
        foreach(var file in b.Files)
        {
            var f=reread.Files.Single(x=>x.Name==file.Name);
            Require(f.ID==file.ID&&f.Flags==file.Flags&&f.CompressionType==file.CompressionType&&f.Bytes.SequenceEqual(file.Bytes),"Binder identity/roundtrip mismatch");
        }
        var allowedEntries=new HashSet<string>{"ItemParam.param","PlayerStatusParam.param","ShopLineupParam.param","SpEffectActiveItem.emevd","ItemLotParam2_Chr.param","ItemLotParam2_Other.param","ItemLotParam2_SvrEvent.param"};
        foreach(var f in b.Files)Require(original[f.Name].SequenceEqual(f.Bytes)||allowedEntries.Contains(Name(f)),"Unrelated archive entry changed");
        var events=EMEVD.Read(activeEntry.Bytes);Require(events.Events.All(e=>e.ID!=Oil),"Oil active event remains");
        Require(events.Events.Count==originalEvents.Count,"Unrelated event count changed");
        foreach(var evt in events.Events)Require(originalEvents[evt.ID]==JsonSerializer.Serialize(evt),"Unrelated active event changed");
        // Independently re-read every edited PARAM from the serialized archive.
        foreach(var entry in reread.Files.Where(f=>Name(f)=="ItemParam.param"||Name(f)=="PlayerStatusParam.param"||Name(f)=="ShopLineupParam.param"||Name(f).StartsWith("ItemLotParam2_")))
        {
            var verify=PARAM.Read(entry.Bytes);verify.ApplyParamdef(defs[verify.ParamType]);
            foreach(var row in verify.Rows)foreach(var c in row.Cells)
                Require(c.Value is byte[]||Cell(c.Value)!=Oil.ToString(),"Oil reference remains in "+Name(entry)+"/"+row.ID+"/"+c.Def.InternalName);
        }
        return new{InputSHA256=expected,OutputSHA256=Hash(File.ReadAllBytes(output)),Path=output,Changes=changes,PreservedLanternAndEstus=true};
    }
    public static void Build(string inputPack,string schemas,string outputPack)
    {
        Require(!Directory.Exists(outputPack),"Candidate already exists");
        Require(!Path.GetFullPath(outputPack).StartsWith(Path.GetFullPath(inputPack),StringComparison.OrdinalIgnoreCase),"Output cannot be inside installed pack");
        var defs=Directory.GetFiles(schemas,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(d=>d.ParamType);
        Directory.CreateDirectory(outputPack);
        foreach(var file in Directory.GetFiles(inputPack,"*",SearchOption.AllDirectories))
        {var target=Path.Combine(outputPack,Path.GetRelativePath(inputPack,file));Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.Copy(file,target);}
        var report=Enumerable.Range(0,4).Select(i=>Regulation(Path.Combine(inputPack,Paths[i]),Path.Combine(outputPack,Paths[i]),Inputs[i],defs)).ToArray();
        var textChanges=new List<string>();
        foreach(var file in Directory.GetFiles(Path.Combine(outputPack,"assets"),"*.fmg",SearchOption.AllDirectories))
        {
            var f=FMG.Read(file);var before=f.Entries.ToDictionary(e=>e.ID,e=>e.Text);bool changed=false;
            foreach(var e in f.Entries)
            {
                if(e.ID==Oil){e.Text=Path.GetFileName(file)=="itemname.fmg"?"Retired item":"";changed=true;}
                if(e.ID==Lantern&&Path.GetFileName(file)=="simpleexplanation.fmg"){e.Text="Reusable wearable lantern. Use to light or stow.";changed=true;}
                if(e.ID==Lantern&&Path.GetFileName(file)=="detailedexplanation.fmg"){e.Text="A reusable lantern worn at the hip.\n\nUse to light or stow. Lighting consumes one Lantern charge; stowing is free.\nRest at a bonfire to refill Lantern charges.";changed=true;}
            }
            if(!changed)continue;
            f.Write(file);var v=FMG.Read(file);Require(v.Entries.Count==before.Count,"FMG entry count changed");
            foreach(var e in v.Entries)Require(e.ID==Oil||e.ID==Lantern||before[e.ID]==e.Text,"Unrelated text changed");
            textChanges.Add(Path.GetRelativePath(outputPack,file));
        }
        var oilIcon=Path.Combine(outputPack,"assets/menu/tex/icon/ic_0060425000.tpf");Require(File.Exists(oilIcon),"Expected Oil icon absent");File.Delete(oilIcon);
        var manifest=Path.Combine(outputPack,"files.tsv");var lines=File.ReadAllLines(manifest);
        File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Where(l=>l.Split('\t')[1]!="menu/tex/icon/ic_0060425000.tpf").Select(l=>{var key=l.Split('\t')[1];return Hash(File.ReadAllBytes(Path.Combine(outputPack,"assets",key))).ToLowerInvariant()+"\t"+key;})));
        var jump=Path.Combine(outputPack,"jump-off.tsv");File.WriteAllText(jump,Hash(File.ReadAllBytes(Path.Combine(outputPack,Paths[1]))).ToLowerInvariant()+"\tvariants/standard-jump.bnd.dcx\n");
        File.WriteAllLines(Path.Combine(outputPack,"estus-off.tsv"),new[]{"jump-on\t"+Hash(File.ReadAllBytes(Path.Combine(outputPack,Paths[2]))).ToLowerInvariant()+"\tvariants/estus-off-jump-on.bnd.dcx","jump-off\t"+Hash(File.ReadAllBytes(Path.Combine(outputPack,Paths[3]))).ToLowerInvariant()+"\tvariants/estus-off-jump-off.bnd.dcx"});
        var ini=Path.Combine(outputPack,"forge.ini");var metadata=File.ReadAllLines(ini).Where(l=>!l.StartsWith("Oil")&&!l.StartsWith("SwordsmanOil")&&!l.StartsWith("PrecreationOil")).ToList();
        metadata.AddRange(new[]{"LanternResource=DS2FX-independent-session-charges","LanternCapacity=3","LanternOilRetired=true","NativeLightMode=owned-slot3-selector-offline-candidate","ChargesPersistence=false"});File.WriteAllLines(ini,metadata);
        File.WriteAllText(Path.Combine(outputPack,"retire-oil.audit.json"),JsonSerializer.Serialize(new{Status="OFFLINE LAB CANDIDATE, not live validated",Regulations=report,TextChanges=textChanges,RemovedIcon="menu/tex/icon/ic_0060425000.tpf",OilSaveCompatibility="Inert row retained; existing inventories not rewritten",EstusUntouched=true},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine("Four regulations round-tripped; Oil awards, shops, starting grants, active events, text and routed icon retired; Lantern and Estus preserved.");
    }
}
