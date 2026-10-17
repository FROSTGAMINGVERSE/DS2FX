using SoulsFormats;
using System.Globalization;
using System.Security.Cryptography;
using System.Text.Json;
static class LanternBowAnimation {
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static void Need(bool v,string s){if(!v)throw new InvalidDataException(s);}
 static string Key(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
 static string Value(object v)=>v is byte[] b?Convert.ToHexString(b):Convert.ToString(v,CultureInfo.InvariantCulture)!;
 static readonly Dictionary<string,string> Inputs=new(){
 ["assets/enc_regulation.bnd.dcx"]="A0E3A43B084B88AE3CC9E0A6F31CAFC61F4862C8BB321DEDBAF20FAE4689AD90",
 ["variants/standard-jump.bnd.dcx"]="0E6CA544EE46C55A8FBFB984B28933D6134BD6306556ED70D9EEA87704EB505E",
 ["variants/estus-off-jump-on.bnd.dcx"]="06C5D13F4B7AF21410F591A990470AFE018DB97422ACE832C4890293C4F5D1D5",
 ["variants/estus-off-jump-off.bnd.dcx"]="3B9A3B0F021161821714B53738F38240FF380B4854C426E0F991FE49F29D641A"};
 public static void Build(string input,string schemas,string typeSchema,string animationBinder,string networkPath,string output){
 input=Path.GetFullPath(input);output=Path.GetFullPath(output);
 Need(!Directory.Exists(output)&&!output.Equals(input,StringComparison.OrdinalIgnoreCase)&&!output.StartsWith(input+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Fresh external output required");
 foreach(var x in Inputs)Need(Hash(File.ReadAllBytes(Path.Combine(input,x.Key)))==x.Value,"Unknown regulation: "+x.Key);
 Need(Hash(File.ReadAllBytes(animationBinder))=="266D1798B25621667755957729A717CAC11608E3422D56C6EC62F479A3916898","Unknown animation binder");
 var network=File.ReadAllBytes(networkPath);Need(Hash(network)=="A82E02AD42B198B860D93FAD600943A37AE3B5D0D022782DF06A069F64C64024","Unknown private network");
 var itemDef=PARAMDEF.XmlDeserialize(Path.Combine(schemas,"ITEM_PARAM.xml"));var def=PARAMDEF.XmlDeserialize(typeSchema);Need(def.ParamType=="ITEM_TYPE_PARAM"&&def.DataVersion==7,"Wrong type schema");
 var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(p=>Path.GetRelativePath(input,p).Replace('\\','/'),p=>Hash(File.ReadAllBytes(p)));
 const string route="morpheme4/chr/c0001.anibnd.dcx";Need(!before.ContainsKey("assets/"+route),"Existing animation route must be merged explicitly");
 foreach(var rel in before.Keys){var dst=Path.Combine(output,rel);Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.Copy(Path.Combine(input,rel),dst);}
 var report=new List<object>();
 foreach(var x in Inputs){
 var binder=BND4.Read(Path.Combine(input,x.Key));var originals=binder.Files.Select(f=>(f.Name,f.ID,f.Flags,f.CompressionType,f.Bytes.ToArray())).ToArray();
 var item=PARAM.Read(binder.Files.Single(f=>Key(f)=="ItemParam.param").Bytes);item.ApplyParamdef(itemDef);var lantern=item.Rows.Single(r=>r.ID==60425001);
 Need(Convert.ToInt32(lantern["itemTypeParamId"].Value)==60425001&&Convert.ToInt32(lantern["itemUsageParamId"].Value)==2200&&Convert.ToInt32(lantern["maxNum"].Value)==1&&Convert.ToInt32(lantern["actionSfxId"].Value)==-1&&Convert.ToSingle(lantern["motionSpeedRate"].Value)==1.2f,"Lantern contract changed");
 Need(item.Rows.Count(r=>Convert.ToInt32(r["itemTypeParamId"].Value)==60425001)==1,"Private type shared");
 var entry=binder.Files.Single(f=>Key(f)=="ItemTypeParam.param");var p=PARAM.Read(entry.Bytes);p.ApplyParamdef(def);
 var baseline=p.Rows.ToDictionary(r=>r.ID,r=>(r.Name,Values:r.Cells.Select(c=>Value(c.Value)).ToArray()));var row=p.Rows.Single(r=>r.ID==60425001);
 Need(Convert.ToInt32(row["Unk10"].Value)==20&&Convert.ToInt32(row["Unk14"].Value)==20&&Convert.ToByte(row["Unk18"].Value)==8,"Private animation changed");
 Need(p.Rows.Where(r=>r.ID!=60425001).All(r=>Convert.ToInt32(r["Unk10"].Value)!=15&&Convert.ToInt32(r["Unk14"].Value)!=15),"Private category15 already used");
 row["Unk10"].Value=15;row["Unk14"].Value=15;entry.Bytes=p.Write();var dst=Path.Combine(output,x.Key);binder.Write(dst);
 var check=BND4.Read(dst);Need(check.Files.Count==originals.Length,"Binder count changed");
 for(int i=0;i<check.Files.Count;i++){var f=check.Files[i];var old=originals[i];Need(f.Name==old.Name&&f.ID==old.ID&&f.Flags==old.Flags&&f.CompressionType==old.CompressionType,"Binder identity changed");Need(f.Bytes.SequenceEqual(binder.Files[i].Bytes),"Binder serialization changed");if(Key(f)!="ItemTypeParam.param")Need(f.Bytes.SequenceEqual(old.Item5),"Unrelated binder data changed");}
 var v=PARAM.Read(check.Files.Single(f=>Key(f)=="ItemTypeParam.param").Bytes);v.ApplyParamdef(def);Need(v.Rows.Count==baseline.Count,"Row count changed");
 foreach(var r in v.Rows){Need(baseline.ContainsKey(r.ID)&&baseline[r.ID].Name==r.Name,"Row identity changed");for(int n=0;n<r.Cells.Count;n++)Need(Value(r.Cells[n].Value)==(r.ID==60425001&&(r.Cells[n].Def.InternalName is "Unk10" or "Unk14")?"15":baseline[r.ID].Values[n]),"Unrelated type field changed: "+r.ID+"/"+r.Cells[n].Def.InternalName);}
 report.Add(new{Path=x.Key,Before=x.Value,After=Hash(File.ReadAllBytes(dst)),From=20,To=15,OnlyChange="ItemTypeParam/60425001/Unk10+Unk14",Speed=1.2});
 }
 var anim=BND4.Read(animationBinder);var animOriginal=anim.Files.Select(f=>(f.Name,f.ID,f.Flags,f.CompressionType,Bytes:f.Bytes.ToArray())).ToArray();var nmb=anim.Files.Single(f=>Key(f)=="c0001.nmb");Need(Hash(nmb.Bytes)=="C2B958E805503F583F8105DC878C51B7E824C846A2BBC9B274E78A0D278951D1","Network base changed");nmb.Bytes=network;
 var animDst=Path.Combine(output,"assets",route);Directory.CreateDirectory(Path.GetDirectoryName(animDst)!);anim.Write(animDst);var ar=BND4.Read(animDst);Need(ar.Files.Count==animOriginal.Length,"Animation binder count changed");
 for(int i=0;i<ar.Files.Count;i++){var f=ar.Files[i];var old=animOriginal[i];Need(f.Name==old.Name&&f.ID==old.ID&&f.Flags==old.Flags&&f.CompressionType==old.CompressionType,"Animation identity changed");Need(f.Bytes.SequenceEqual(Key(f)=="c0001.nmb"?network:old.Bytes),"Animation payload changed");}
 var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);Need(!lines.Skip(1).Any(l=>l.Split('\t')[1]==route),"Duplicate animation manifest key");File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Select(l=>{var key=l.Split('\t')[1];return Hash(File.ReadAllBytes(Path.Combine(output,"assets",key))).ToLowerInvariant()+"\t"+key;})).Append(Hash(File.ReadAllBytes(animDst)).ToLowerInvariant()+"\t"+route));
 File.WriteAllText(Path.Combine(output,"jump-off.tsv"),Hash(File.ReadAllBytes(Path.Combine(output,"variants/standard-jump.bnd.dcx"))).ToLowerInvariant()+"\tvariants/standard-jump.bnd.dcx\n");
 File.WriteAllLines(Path.Combine(output,"estus-off.tsv"),new[]{"jump-on\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-on.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-on.bnd.dcx","jump-off\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-off.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-off.bnd.dcx"});
 var forge=Path.Combine(output,"forge.ini");File.WriteAllText(forge,File.ReadAllText(forge).Replace("LanternAnimationCandidate=repair-powder-index-20","LanternAnimationCandidate=empty-bow-private-index-15"));
 var allowed=Inputs.Keys.Concat(new[]{"files.tsv","jump-off.tsv","estus-off.tsv","forge.ini"}).ToHashSet();foreach(var x in before)if(!allowed.Contains(x.Key))Need(Hash(File.ReadAllBytes(Path.Combine(output,x.Key)))==x.Value,"Unrelated pack file changed: "+x.Key);
 File.WriteAllText(Path.Combine(output,"lantern-bow-animation.audit.json"),JsonSerializer.Serialize(new{Regulations=report,AnimationAsset=route,AnimationSHA256=Hash(File.ReadAllBytes(animDst)),NetworkSHA256=Hash(network),PrivateCategory=15,ClipAsset=559,Motion=40215030,Speed=1.2,ItemUsageAndActiveEventUnchanged=true,LiveValidated=false},new JsonSerializerOptions{WriteIndented=true}));
 Console.WriteLine("Four variants verified; private category15 selects the owned empty-bow leaf. Original clips, item usage/events and speed1.2 preserved.");
 }
}
