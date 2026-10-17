using System.Globalization;
using System.Security.Cryptography;
using System.Text.Json;
using System.Xml.Linq;
using SoulsFormats;

// A deliberately isolated content experiment. Parameter storage is traced in
// the exact Scholar binary; the extent interpretation still needs visual QA.
internal static class CompactFlame
{
    internal const int EffectId = 6042;
    const string SourceHash = "27871CA29E6A0E9C35F6CC13E2B5E1F1A2FD683A0AD5BE8724D02B78C24A9245";
    const string TorchHash = "CB918BB2A711C5574CA130CD9EB7FE96A031D1AC8890D1EE7B68F672D13EBEB3";
    static string Hash(byte[] b) => Convert.ToHexString(SHA256.HashData(b));
    static XDocument Xml(FFXDLSE effect) { using var s = new MemoryStream(); effect.XmlSerialize(s); s.Position=0; return XDocument.Load(s); }

    internal static void Build(string input, string output, bool centered=false)
    {
        output=Path.GetFullPath(output);
        if (!output.StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\", StringComparison.OrdinalIgnoreCase)) throw new Exception("Research output required.");
        if (File.Exists(output)) throw new Exception("Output exists; preserve previous candidate.");
        var source=File.ReadAllBytes(input);
        if (Hash(source)!=SourceHash) throw new Exception("Unknown SFX baseline; explicit merge required.");
        var binder=BND4.Read(source);
        var original=binder.Files.Select(f=>new{f.ID,f.Name,f.Flags,f.CompressionType,Hash=Hash(f.Bytes)}).ToArray();
        var torch=binder.Files.Single(f=>f.Name.Replace('\\','/').EndsWith("/f0000230.ffx",StringComparison.Ordinal));
        if (Hash(torch.Bytes)!=TorchHash) throw new Exception("Unknown torch effect.");
        var newName=torch.Name.Replace("f0000230.ffx",$"f{EffectId:D7}.ffx");
        if (binder.Files.Any(f=>f.ID==EffectId||f.Name.Equals(newName,StringComparison.OrdinalIgnoreCase))) throw new Exception("Owned SFX identity already used.");
        var xml=Xml(FFXDLSE.Read(torch.Bytes));
        var oldXml=new XDocument(xml);
        xml.Root!.Element("Effect")!.SetAttributeValue("ID",EffectId);
        var changed=new List<object>();
        foreach(var spec in new[]{(Id:15000,Fields:new[]{10,11}),(Id:15001,Fields:new[]{19,20}),(Id:20002,Fields:new[]{10,11})})
        {
            var action=xml.Descendants("Param").Single(p=>(string?)p.Attribute("ActionID")==spec.Id.ToString(CultureInfo.InvariantCulture));
            var fields=action.Element("ParamList")!.Elements("Param").ToArray();
            foreach(var index in spec.Fields)
            {
                if ((string?)fields[index].Attribute(XName.Get("type","http://www.w3.org/2001/XMLSchema-instance"))!="Param11") throw new Exception("Unexpected extent candidate type.");
                var values=fields[index].Descendants("TickFloat").ToArray();
                if(values.Length is <1 or >2) throw new Exception("Unexpected donor curve.");
                foreach(var value in values)
                {
                    float before=float.Parse(value.Attribute("Float")!.Value,CultureInfo.InvariantCulture);
                    float after=before*0.16f;
                    changed.Add(new{Action=spec.Id,Field=index,Tick=(string?)value.Attribute("Tick"),Before=before,After=after});
                    value.SetAttributeValue("Float",after.ToString("R",CultureInfo.InvariantCulture));
                }
            }
        }
        if(changed.Count!=8) throw new Exception("Unexpected exact edit count.");
        if(centered)
        {
            // Authored DS1/DSR FFX references identify these as spatial emitter
            // and movement modifiers. Scholar interpretation remains empirical:
            // v8's small sprites still leave the lantern, so retain their shape
            // but shorten the donor torch's offset and travel. No rotations,
            // timing, resource IDs, nesting or unrelated action fields change.
            var transforms=xml.Descendants("Param").Where(p=>(string?)p.Attribute("ActionID")=="35").ToArray();
            if(transforms.Length!=4)throw new Exception("Unexpected transform layout.");
            var glow=transforms[0].Element("ParamList")!.Elements("Param").ElementAt(1).Attribute("Float")!;
            if(glow.Value!="0.15")throw new Exception("Unexpected glow offset.");
            glow.Value="0.024";changed.Add(new{Action=35,Occurrence=0,Field=1,Before=0.15f,After=0.024f});
            var movements=xml.Descendants("Param").Where(p=>(string?)p.Attribute("ActionID")=="105").ToArray();
            if(movements.Length!=2)throw new Exception("Unexpected movement layout.");
            for(int i=0;i<movements.Length;i++)
            {
                var value=movements[i].Element("ParamList")!.Elements("Param").First().Descendants("TickFloat").Single().Attribute("Float")!;
                float before=float.Parse(value.Value,CultureInfo.InvariantCulture);
                if(before!=(i==0?-0.1f:-0.2f))throw new Exception("Unexpected donor movement.");
                float after=before*0.06f;value.Value=after.ToString("R",CultureInfo.InvariantCulture);
                changed.Add(new{Action=105,Occurrence=i,Field=0,Before=before,After=after});
            }
            var cone=xml.Descendants("Param").Single(p=>(string?)p.Attribute("ActionID")=="28");
            var extent=cone.Element("ParamList")!.Elements("Param").ElementAt(3).Descendants("TickFloat3").Single().Element("Float1")!;
            if(extent.Value!="0.125")throw new Exception("Unexpected donor cone extent.");
            extent.Value="0.0075";changed.Add(new{Action=28,Occurrence=0,Field=3,Before=0.125f,After=0.0075f});
            if(changed.Count!=12)throw new Exception("Unexpected centered edit count.");
        }
        using var reader=xml.CreateReader();
        var candidate=FFXDLSE.XmlDeserialize(reader).Write();
        if(!XNode.DeepEquals(xml,Xml(FFXDLSE.Read(candidate)))) throw new Exception("FFX semantic round-trip mismatch.");
        // Undo exactly our changes in a second document: no hidden edits allowed.
        var restored=Xml(FFXDLSE.Read(candidate));
        restored.Root!.Element("Effect")!.SetAttributeValue("ID",230);
        foreach(var spec in new[]{(Id:15000,Fields:new[]{10,11}),(Id:15001,Fields:new[]{19,20}),(Id:20002,Fields:new[]{10,11})})
        {
            var key=spec.Id.ToString(CultureInfo.InvariantCulture);
            var a=restored.Descendants("Param").Single(p=>(string?)p.Attribute("ActionID")==key).Element("ParamList")!.Elements("Param").ToArray();
            var b=oldXml.Descendants("Param").Single(p=>(string?)p.Attribute("ActionID")==key).Element("ParamList")!.Elements("Param").ToArray();
            foreach(var index in spec.Fields) a[index].ReplaceWith(new XElement(b[index]));
        }
        if(centered)foreach(var id in new[]{"35","105","28"})
        {
            var a=restored.Descendants("Param").Where(p=>(string?)p.Attribute("ActionID")==id).ToArray();
            var b=oldXml.Descendants("Param").Where(p=>(string?)p.Attribute("ActionID")==id).ToArray();
            if(a.Length!=b.Length)throw new Exception("Changed action structure.");
            for(int i=0;i<a.Length;i++)a[i].ReplaceWith(new XElement(b[i]));
        }
        if(!XNode.DeepEquals(oldXml,restored)) throw new Exception("Unrelated FFX semantics changed.");
        binder.Files.Add(new BinderFile(torch.Flags,EffectId,newName,candidate){CompressionType=torch.CompressionType});
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        binder.Write(output);
        var check=BND4.Read(output);
        if(check.Files.Count!=original.Length+1) throw new Exception("SFX entry count mismatch.");
        for(int i=0;i<original.Length;i++)
        {
            var before=original[i];var after=check.Files[i];
            if(before.ID!=after.ID||before.Name!=after.Name||before.Flags!=after.Flags||before.CompressionType!=after.CompressionType||before.Hash!=Hash(after.Bytes)) throw new Exception("Original SFX entry changed.");
        }
        if(Hash(check.Files.Last().Bytes)!=Hash(candidate))throw new Exception("Candidate payload mismatch.");
        xml.Save(output+".effect.xml");
        var audit=new{Role=centered?"empirical-compact-emitter-candidate-not-verified":"empirical-billboard-extent-candidate-not-verified",InputSHA256=SourceHash,OutputSHA256=Hash(File.ReadAllBytes(output)),EffectId,EffectSHA256=Hash(candidate),Scale=0.16,OriginalEntriesPreserved=original.Length,Torch230ByteIdentical=true,UnrelatedSemanticsPreserved=true,Changes=changed,MotionUnchanged=!centered,SoundUnchanged=true,FieldMeaningFullyProven=false,LiveVerified=false,Deployed=false};
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine(JsonSerializer.Serialize(audit));
    }
}
