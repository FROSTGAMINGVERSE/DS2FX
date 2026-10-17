using SoulsFormats;
using System.Security.Cryptography;
using System.Text.Json;
using System.Globalization;

// Remove only the inherited activation SFX; preserve the live-tested item route.
static class LanternUseEffectRemoval
{
 const int Lantern=60425001;
 static readonly Dictionary<string,string> Inputs=new(){
  ["assets/enc_regulation.bnd.dcx"]="A6FD10135E0C273C2E4C590947F697635AABA724CFF6D645FA30AA00E60A8B90",
  ["variants/standard-jump.bnd.dcx"]="FA8EF275054F8F1B20CC9211F5C3FB446E8303260CCEEB1AF7C6EE4F468D034D",
  ["variants/estus-off-jump-on.bnd.dcx"]="142BB0156C387178398318B10CD08AA6F7ED270C0E86E73EFEF7E0E57545BC7A",
  ["variants/estus-off-jump-off.bnd.dcx"]="54F34A1A6AB331101E552371B4CE57CB197ACE79B953705F3218DB9E14397D64"};
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static void Need(bool ok,string m){if(!ok)throw new InvalidDataException(m);}
 static string Key(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
 static string Value(object v)=>v is byte[] b?Convert.ToHexString(b):Convert.ToString(v,CultureInfo.InvariantCulture)!;
 public static void Build(string input,string schemas,string output){
  input=Path.GetFullPath(input);output=Path.GetFullPath(output);
  Need(!Directory.Exists(output)&&!output.Equals(input,StringComparison.OrdinalIgnoreCase)&&!output.StartsWith(input+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Fresh external output required");
  foreach(var x in Inputs)Need(Hash(File.ReadAllBytes(Path.Combine(input,x.Key)))==x.Value,"Unknown regulation: "+x.Key);
  var def=PARAMDEF.XmlDeserialize(Path.Combine(schemas,"ITEM_PARAM.xml"));
  var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(p=>Path.GetRelativePath(input,p).Replace('\\','/'),p=>Hash(File.ReadAllBytes(p)));
  foreach(var rel in before.Keys){var dst=Path.Combine(output,rel);Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.Copy(Path.Combine(input,rel),dst);}
  var report=new List<object>();
  foreach(var x in Inputs){
   var binder=BND4.Read(Path.Combine(input,x.Key));
   var entries=binder.Files.Select(f=>(f.Name,f.ID,f.Flags,f.CompressionType,f.Bytes.ToArray())).ToArray();
   var entry=binder.Files.Single(f=>Key(f)=="ItemParam.param");var p=PARAM.Read(entry.Bytes);p.ApplyParamdef(def);
   var baseline=p.Rows.ToDictionary(r=>r.ID,r=>(r.Name,Values:r.Cells.Select(c=>Value(c.Value)).ToArray()));
   var row=p.Rows.Single(r=>r.ID==Lantern);
   Need(Convert.ToInt32(row["itemTypeParamId"].Value)==Lantern&&Convert.ToInt32(row["itemUsageParamId"].Value)==2200&&Convert.ToInt32(row["maxNum"].Value)==1&&Convert.ToSingle(row["motionSpeedRate"].Value)==2.0f&&Convert.ToInt32(row["specialEffectId"].Value)==Lantern,"Working Lantern contract changed");
   Need(Convert.ToInt32(row["actionSfxId"].Value)==864,"Unexpected inherited action SFX");
   row["actionSfxId"].Value=-1;entry.Bytes=p.Write();var dst=Path.Combine(output,x.Key);binder.Write(dst);
   var check=BND4.Read(dst);Need(check.Files.Count==entries.Length,"Binder entry count changed");
   for(int i=0;i<check.Files.Count;i++){
    var f=check.Files[i];var old=entries[i];
    Need(f.Name==old.Name&&f.ID==old.ID&&f.Flags==old.Flags&&f.CompressionType==old.CompressionType,"Binder identity/order changed");
    if(Key(f)!="ItemParam.param")Need(old.Item5.SequenceEqual(f.Bytes),"Unrelated binder payload changed: "+f.Name);
   }
   var verify=PARAM.Read(check.Files.Single(f=>Key(f)=="ItemParam.param").Bytes);verify.ApplyParamdef(def);
   Need(verify.Rows.Count==baseline.Count,"Item row count changed");
   foreach(var r in verify.Rows){
    Need(baseline.ContainsKey(r.ID)&&r.Name==baseline[r.ID].Name,"Item identity changed");
    for(int i=0;i<r.Cells.Count;i++)Need(Value(r.Cells[i].Value)==(r.ID==Lantern&&r.Cells[i].Def.InternalName=="actionSfxId"?"-1":baseline[r.ID].Values[i]),"Unrelated item field changed: "+r.ID+"/"+r.Cells[i].Def.InternalName);
   }
   report.Add(new{Path=x.Key,Before=x.Value,After=Hash(File.ReadAllBytes(dst)),OnlyChange="ItemParam/60425001/actionSfxId",From=864,To=-1});
  }
  var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);
  File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Select(l=>{var key=l.Split('\t')[1];return Hash(File.ReadAllBytes(Path.Combine(output,"assets",key))).ToLowerInvariant()+"\t"+key;})));
  File.WriteAllText(Path.Combine(output,"jump-off.tsv"),Hash(File.ReadAllBytes(Path.Combine(output,"variants/standard-jump.bnd.dcx"))).ToLowerInvariant()+"\tvariants/standard-jump.bnd.dcx\n");
  File.WriteAllLines(Path.Combine(output,"estus-off.tsv"),new[]{"jump-on\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-on.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-on.bnd.dcx","jump-off\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-off.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-off.bnd.dcx"});
  var allowed=Inputs.Keys.Concat(new[]{"files.tsv","jump-off.tsv","estus-off.tsv"}).ToHashSet();
  foreach(var x in before)if(!allowed.Contains(x.Key))Need(Hash(File.ReadAllBytes(Path.Combine(output,x.Key)))==x.Value,"Unrelated pack file changed: "+x.Key);
  File.WriteAllText(Path.Combine(output,"lantern-use-effect.audit.json"),JsonSerializer.Serialize(new{Regulations=report,ShortAnimationUserConfirmed=true,ShortAnimationTypeByteIdentical=true,UsageAndActiveEventByteIdentical=true,FlameModelIconByteIdentical=true,CoreUnchanged=true,EffectRemovalLiveValidated=false},new JsonSerializerOptions{WriteIndented=true}));
  Console.WriteLine("Four variants verified: only Lantern actionSfxId864 -> -1. Short animation, reuse, toggle event, native flame/light and all other items preserved.");
 }
}
