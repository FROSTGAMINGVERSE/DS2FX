using SoulsFormats;
using System.Globalization;
using System.Security.Cryptography;
using System.Text.Json;
static class LanternAnimationRate {
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static void Need(bool v,string s){if(!v)throw new InvalidDataException(s);}
 static string Key(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
 static string Value(object v)=>v is byte[] b?Convert.ToHexString(b):Convert.ToString(v,CultureInfo.InvariantCulture)!;
 static readonly Dictionary<string,string> Inputs=new(){
 ["assets/enc_regulation.bnd.dcx"]="D75DD4FCEE5C501B4C4AD09189452A0DD3A0D16949342BEAD8E76DAEFAAEA54E",
 ["variants/standard-jump.bnd.dcx"]="3971BFC2E79F4256672B90C72CED569636ED365F41D5517BD3444F1E7DD400F9",
 ["variants/estus-off-jump-on.bnd.dcx"]="689D7B6BFC7480C47ADB320B56D82479D5B58C20D7BEE1581070A6CC10B695E2",
 ["variants/estus-off-jump-off.bnd.dcx"]="937E3FA9D6643769FE823E828CBCFC5033BDCF9ED2D2C49A4684FDE0BBCD5386"};
 public static void Build(string input,string schemas,string output){
 input=Path.GetFullPath(input);output=Path.GetFullPath(output);
 Need(!Directory.Exists(output)&&!output.Equals(input,StringComparison.OrdinalIgnoreCase)&&!output.StartsWith(input+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Fresh external output required");
 foreach(var x in Inputs)Need(Hash(File.ReadAllBytes(Path.Combine(input,x.Key)))==x.Value,"Unknown regulation: "+x.Key);
 var def=PARAMDEF.XmlDeserialize(Path.Combine(schemas,"ITEM_PARAM.xml"));
 var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(p=>Path.GetRelativePath(input,p).Replace('\\','/'),p=>Hash(File.ReadAllBytes(p)));
 foreach(var rel in before.Keys){var dst=Path.Combine(output,rel);Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.Copy(Path.Combine(input,rel),dst);}
 var report=new List<object>();
 foreach(var x in Inputs){
 var binder=BND4.Read(Path.Combine(input,x.Key));var originals=binder.Files.Select(f=>(f.Name,f.ID,f.Flags,f.CompressionType,f.Bytes.ToArray())).ToArray();
 var entry=binder.Files.Single(f=>Key(f)=="ItemParam.param");var p=PARAM.Read(entry.Bytes);p.ApplyParamdef(def);
 var baseline=p.Rows.ToDictionary(r=>r.ID,r=>(r.Name,Values:r.Cells.Select(c=>Value(c.Value)).ToArray()));var row=p.Rows.Single(r=>r.ID==60425001);
 Need(Convert.ToInt32(row["itemTypeParamId"].Value)==60425001&&Convert.ToInt32(row["itemUsageParamId"].Value)==2200&&Convert.ToInt32(row["maxNum"].Value)==1&&Convert.ToInt32(row["actionSfxId"].Value)==-1,"Lantern contract changed");
 Need(Convert.ToSingle(row["motionSpeedRate"].Value)==2.0f,"Unexpected prior animation rate");row["motionSpeedRate"].Value=1.2f;entry.Bytes=p.Write();var dst=Path.Combine(output,x.Key);binder.Write(dst);
 var check=BND4.Read(dst);Need(check.Files.Count==originals.Length,"Binder count changed");
 for(int i=0;i<check.Files.Count;i++){var f=check.Files[i];var old=originals[i];Need(f.Name==old.Name&&f.ID==old.ID&&f.Flags==old.Flags&&f.CompressionType==old.CompressionType,"Binder identity changed");Need(f.Bytes.SequenceEqual(binder.Files[i].Bytes),"Binder serialization changed");if(Key(f)!="ItemParam.param")Need(f.Bytes.SequenceEqual(old.Item5),"Unrelated binder data changed");}
 var v=PARAM.Read(check.Files.Single(f=>Key(f)=="ItemParam.param").Bytes);v.ApplyParamdef(def);Need(v.Rows.Count==baseline.Count,"Row count changed");
 foreach(var r in v.Rows){Need(baseline.ContainsKey(r.ID)&&baseline[r.ID].Name==r.Name,"Row identity changed");for(int n=0;n<r.Cells.Count;n++)Need(Value(r.Cells[n].Value)==(r.ID==60425001&&r.Cells[n].Def.InternalName=="motionSpeedRate"?"1.2":baseline[r.ID].Values[n]),"Unrelated field changed: "+r.ID+"/"+r.Cells[n].Def.InternalName);}
 report.Add(new{Path=x.Key,Before=x.Value,After=Hash(File.ReadAllBytes(dst)),From=2.0,To=1.2,OnlyChange="ItemParam/60425001/motionSpeedRate"});
 }
 var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Select(l=>{var key=l.Split('\t')[1];return Hash(File.ReadAllBytes(Path.Combine(output,"assets",key))).ToLowerInvariant()+"\t"+key;})));
 File.WriteAllText(Path.Combine(output,"jump-off.tsv"),Hash(File.ReadAllBytes(Path.Combine(output,"variants/standard-jump.bnd.dcx"))).ToLowerInvariant()+"\tvariants/standard-jump.bnd.dcx\n");
 File.WriteAllLines(Path.Combine(output,"estus-off.tsv"),new[]{"jump-on\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-on.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-on.bnd.dcx","jump-off\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-off.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-off.bnd.dcx"});
 var allowed=Inputs.Keys.Concat(new[]{"files.tsv","jump-off.tsv","estus-off.tsv"}).ToHashSet();foreach(var x in before)if(!allowed.Contains(x.Key))Need(Hash(File.ReadAllBytes(Path.Combine(output,x.Key)))==x.Value,"Unrelated pack file changed: "+x.Key);
 File.WriteAllText(Path.Combine(output,"lantern-animation-rate.audit.json"),JsonSerializer.Serialize(new{Regulations=report,ActiveEventAndTypeAndUsageUnchanged=true,BowAnimationApplied=false,LiveValidated=false},new JsonSerializerOptions{WriteIndented=true}));
 Console.WriteLine("Four variants verified; only the Lantern animation-rate field changed from2.0 to1.2. Bow mapping is a separate change.");
 }
}
