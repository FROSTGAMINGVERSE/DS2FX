using SoulsFormats;
using System.Security.Cryptography;
using System.Text.Json;
using System.Globalization;

// Presentation-only candidate: native reusable contract and active event retained.
static class LanternPolish
{
 const int Lantern=60425001;
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static void Need(bool ok,string m){if(!ok)throw new InvalidDataException(m);}
 static string Key(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
 static string Value(object v)=>v is byte[] b?Convert.ToHexString(b):Convert.ToString(v,CultureInfo.InvariantCulture)!;
 static readonly Dictionary<string,string> Inputs=new(){
  ["assets/enc_regulation.bnd.dcx"]="94BE28F184793F9B5B17EC3E01753E5DCA9F49CF1DF8CB159F099603771E326C",
  ["variants/standard-jump.bnd.dcx"]="AE16A3AA5BFFDEBACAA4CE4B7D23B9C13BBB4AA0CDC7B7BC4EC02A41983A4130",
  ["variants/estus-off-jump-on.bnd.dcx"]="BBEDABE07B5479C8743B9B2EFA291300A0DA716098DEBC0B5C6F1C528DD2BC83",
  ["variants/estus-off-jump-off.bnd.dcx"]="7D8A8B755DD007297CA1C93605A930AA73D0DC97E2244442400F79DC156C3272"};
 static byte[] Icon(string p){var b=File.ReadAllBytes(p);Need(Hash(b)=="FE946E2B83C2C0517B5F285097591F011F8184529AEAE74B6EF1CF8F2A8A37FA","Unknown original Lantern icon");return b;}
 public static void Extract(string input,string output){Need(!File.Exists(output),"DDS output exists");var t=TPF.Read(Icon(input));Need(t.Textures.Count==1&&t.Textures[0].Name=="IT_IC_0060425001","Icon identity changed");File.WriteAllBytes(output,t.Textures[0].Bytes);}
 static void Contract(PARAM.Row r){Need(Convert.ToInt32(r["itemTypeParamId"].Value)==260&&Convert.ToInt32(r["itemUsageParamId"].Value)==2200&&Convert.ToInt32(r["maxNum"].Value)==1&&Convert.ToInt32(r["specialEffectId"].Value)==Lantern&&Convert.ToInt32(r["iconId"].Value)==Lantern,"Reusable item contract changed");}
 public static void Build(string input,string schemas,string ddsPath,string output){
  input=Path.GetFullPath(input);output=Path.GetFullPath(output);Need(!Directory.Exists(output)&&!output.StartsWith(input,StringComparison.OrdinalIgnoreCase),"Fresh external output required");
  foreach(var x in Inputs)Need(Hash(File.ReadAllBytes(Path.Combine(input,x.Key)))==x.Value,"Unknown regulation: "+x.Key);
  var defs=Directory.GetFiles(schemas,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(d=>d.ParamType);
  var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(p=>Path.GetRelativePath(input,p).Replace('\\','/'),p=>Hash(File.ReadAllBytes(p)));
  foreach(var rel in before.Keys){var dst=Path.Combine(output,rel);Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.Copy(Path.Combine(input,rel),dst);}
  var report=new List<object>();
  foreach(var x in Inputs){
   var dst=Path.Combine(output,x.Key);var binder=BND4.Read(Path.Combine(input,x.Key));var original=binder.Files.ToDictionary(f=>f.Name,f=>f.Bytes.ToArray());
   var entry=binder.Files.Single(f=>Key(f)=="ItemParam.param");var p=PARAM.Read(entry.Bytes);p.ApplyParamdef(defs[p.ParamType]);
   var baseline=p.Rows.ToDictionary(r=>r.ID,r=>r.Cells.Select(c=>Value(c.Value)).ToArray());var row=p.Rows.Single(r=>r.ID==Lantern);Contract(row);
   Need(Convert.ToSingle(row["motionSpeedRate"].Value)==0.7f,"Unexpected Lantern speed");row["motionSpeedRate"].Value=2.0f;entry.Bytes=p.Write();binder.Write(dst);
   var reread=BND4.Read(dst);Need(reread.Files.Count==binder.Files.Count,"Binder entry count changed");
   foreach(var f in reread.Files.Where(f=>Key(f)!="ItemParam.param"))Need(original[f.Name].SequenceEqual(f.Bytes),"Unrelated binder entry changed: "+f.Name);
   var v=PARAM.Read(reread.Files.Single(f=>Key(f)=="ItemParam.param").Bytes);v.ApplyParamdef(defs[v.ParamType]);Need(v.Rows.Count==p.Rows.Count,"Item row count changed");
   foreach(var r in v.Rows)for(int n=0;n<r.Cells.Count;n++)Need(Value(r.Cells[n].Value)==(r.ID==Lantern&&r.Cells[n].Def.InternalName=="motionSpeedRate"?"2":baseline[r.ID][n]),"Unrelated item field changed");
   Contract(v.Rows.Single(r=>r.ID==Lantern));
   report.Add(new{Path=x.Key,Before=x.Value,After=Hash(File.ReadAllBytes(dst)),OnlyChange="ItemParam/60425001/motionSpeedRate",From=0.7,To=2.0});
  }
  const string icon="assets/menu/tex/icon/ic_0060425001.tpf";var tpf=TPF.Read(Icon(Path.Combine(input,icon)));var dds=File.ReadAllBytes(ddsPath);
  Need(dds.Length==8320&&System.Text.Encoding.ASCII.GetString(dds,0,4)=="DDS "&&BitConverter.ToInt32(dds,12)==128&&BitConverter.ToInt32(dds,16)==64&&System.Text.Encoding.ASCII.GetString(dds,84,4)=="DXT5","Expected original64x128 one-mip BC3 dimensions");
  tpf.Textures[0].Bytes=dds;tpf.Write(Path.Combine(output,icon));var vt=TPF.Read(Path.Combine(output,icon));Need(vt.Textures.Count==1&&vt.Textures[0].Name=="IT_IC_0060425001"&&vt.Textures[0].Bytes.SequenceEqual(dds),"Icon round-trip failed");
  var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Select(l=>{var key=l.Split('\t')[1];return Hash(File.ReadAllBytes(Path.Combine(output,"assets",key))).ToLowerInvariant()+"\t"+key;})));
  File.WriteAllText(Path.Combine(output,"jump-off.tsv"),Hash(File.ReadAllBytes(Path.Combine(output,"variants/standard-jump.bnd.dcx"))).ToLowerInvariant()+"\tvariants/standard-jump.bnd.dcx\n");
  File.WriteAllLines(Path.Combine(output,"estus-off.tsv"),new[]{"jump-on\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-on.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-on.bnd.dcx","jump-off\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-off.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-off.bnd.dcx"});
  File.AppendAllLines(Path.Combine(output,"forge.ini"),new[]{"LanternUseMotionSpeed=2.0","LanternIconArtworkScale=0.72"});
  var allowed=Inputs.Keys.Concat(new[]{icon,"files.tsv","jump-off.tsv","estus-off.tsv","forge.ini"}).ToHashSet();foreach(var x in before)if(!allowed.Contains(x.Key))Need(Hash(File.ReadAllBytes(Path.Combine(output,x.Key)))==x.Value,"Unrelated pack file changed: "+x.Key);
  File.WriteAllText(Path.Combine(output,"lantern-polish.audit.json"),JsonSerializer.Serialize(new{Regulations=report,SourceIcon=before[icon],Icon=Hash(File.ReadAllBytes(Path.Combine(output,icon))),EstusUntouched=true,ReusableContractPreserved=true,ActiveEventUnchanged=true,IconArtworkScale=0.72,LiveValidated=false},new JsonSerializerOptions{WriteIndented=true}));
  Console.WriteLine("Four variants round-tripped; only Lantern motionSpeedRate changed0.7->2.0. Original icon container preserved; unrelated content unchanged.");
 }
}
