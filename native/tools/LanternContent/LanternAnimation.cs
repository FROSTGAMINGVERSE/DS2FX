using SoulsFormats;
using System.Globalization;
using System.Security.Cryptography;
using System.Text.Json;

// Isolated Lab experiment: owned Darksign type clone, animation indices only.
static class LanternAnimation
{
 const int Lantern=60425001;
 static readonly Dictionary<string,string> Inputs=new(){
  ["assets/enc_regulation.bnd.dcx"]="61C223BDC3419E9F2DDF90C17766C48E3906B4C2310484C0B8341707C9DCA4A0",
  ["variants/standard-jump.bnd.dcx"]="A62BB2747D5B9799451283E7FF4B49318C3A494CFE006B355429126CB3304B87",
  ["variants/estus-off-jump-on.bnd.dcx"]="26D7E9F50AA7BAEC6C7B2E47FBB86BD0C47DDF644683CD5F6ACE9BD9A96E5431",
  ["variants/estus-off-jump-off.bnd.dcx"]="2899640708FAFF7935410BE8DF477AA4CF7B668CA2C93645C86B72F582F10FC4"};
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static void Need(bool ok,string m){if(!ok)throw new InvalidDataException(m);}
 static string Key(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
 static string Value(object v)=>v is byte[] b?Convert.ToHexString(b):Convert.ToString(v,CultureInfo.InvariantCulture)!;
 static Dictionary<int,string[]> Snapshot(PARAM p)=>p.Rows.ToDictionary(r=>r.ID,r=>r.Cells.Select(c=>Value(c.Value)).ToArray());
 static void Validate(PARAM p,Dictionary<int,string[]> before,bool item){
  Need(p.Rows.Count==before.Count+(item?0:1),"Row count changed unexpectedly");
  foreach(var r in p.Rows){
   if(!item&&r.ID==Lantern)continue;
   Need(before.ContainsKey(r.ID),"Unexpected row");
   for(int i=0;i<r.Cells.Count;i++)Need(Value(r.Cells[i].Value)==(item&&r.ID==Lantern&&r.Cells[i].Def.InternalName=="itemTypeParamId"?Lantern.ToString():before[r.ID][i]),"Unrelated field changed: "+r.ID+"/"+r.Cells[i].Def.InternalName);
  }
 }
 public static void Build(string input,string schemas,string typeSchema,string output){
  input=Path.GetFullPath(input);output=Path.GetFullPath(output);
  Need(!Directory.Exists(output)&&!output.StartsWith(input+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Fresh external output required");
  foreach(var x in Inputs)Need(Hash(File.ReadAllBytes(Path.Combine(input,x.Key)))==x.Value,"Unknown regulation: "+x.Key);
  var itemDef=PARAMDEF.XmlDeserialize(Path.Combine(schemas,"ITEM_PARAM.xml"));
  var typeDef=PARAMDEF.XmlDeserialize(typeSchema);
  Need(typeDef.ParamType=="ITEM_TYPE_PARAM"&&typeDef.DataVersion==7&&typeDef.Fields.Count==10,"Unexpected item-type definition");
  Need(typeDef.Fields.Select(f=>f.InternalName).SequenceEqual(new[]{"Unk00","Unk04","Unk08","Unk0C","Unk10","Unk14","Unk18","Unk19","Unk20","Unk21"}),"Unexpected item-type layout");
  var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(p=>Path.GetRelativePath(input,p).Replace('\\','/'),p=>Hash(File.ReadAllBytes(p)));
  foreach(var rel in before.Keys){var dst=Path.Combine(output,rel);Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.Copy(Path.Combine(input,rel),dst);}
  var report=new List<object>();
  foreach(var x in Inputs){
   var binder=BND4.Read(Path.Combine(input,x.Key));var entries=binder.Files.ToDictionary(f=>f.Name,f=>(f.ID,f.Flags,f.Bytes.ToArray()));
   var itemEntry=binder.Files.Single(f=>Key(f)=="ItemParam.param");var typeEntry=binder.Files.Single(f=>Key(f)=="ItemTypeParam.param");
   var items=PARAM.Read(itemEntry.Bytes);items.ApplyParamdef(itemDef);var types=PARAM.Read(typeEntry.Bytes);types.ApplyParamdef(typeDef);
   var bi=Snapshot(items);var bt=Snapshot(types);var row=items.Rows.Single(r=>r.ID==Lantern);
   Need(Convert.ToInt32(row["itemTypeParamId"].Value)==260&&Convert.ToInt32(row["itemUsageParamId"].Value)==2200&&Convert.ToInt32(row["maxNum"].Value)==1&&Convert.ToSingle(row["motionSpeedRate"].Value)==2.0f,"Unexpected reusable Lantern input");
   Need(!types.Rows.Any(r=>r.ID==Lantern)&&!items.Rows.Any(r=>Convert.ToInt32(r["itemTypeParamId"].Value)==Lantern),"Owned type key already used");
   var source=types.Rows.Single(r=>r.ID==260);var powder=types.Rows.Single(r=>r.ID==700);
   Need(Convert.ToInt32(source["Unk10"].Value)==210&&Convert.ToInt32(source["Unk14"].Value)==210&&Convert.ToByte(source["Unk18"].Value)==8,"Darksign type changed");
   Need(Convert.ToInt32(powder["Unk10"].Value)==20&&Convert.ToInt32(powder["Unk14"].Value)==20,"Repair Powder animation changed");
   var owned=new PARAM.Row(Lantern,"DS2FX Lantern - Darksign flags / Repair Powder motion",typeDef);
   for(int i=0;i<source.Cells.Count;i++)owned.Cells[i].Value=source.Cells[i].Value;
   owned["Unk10"].Value=20;owned["Unk14"].Value=20;
   types.Rows.Add(owned);types.Rows.Sort((a,b)=>a.ID.CompareTo(b.ID));row["itemTypeParamId"].Value=Lantern;
   itemEntry.Bytes=items.Write();typeEntry.Bytes=types.Write();var dst=Path.Combine(output,x.Key);binder.Write(dst);
   var check=BND4.Read(dst);Need(check.Files.Count==binder.Files.Count,"Binder entry count changed");
   foreach(var f in check.Files){var old=entries[f.Name];Need(f.ID==old.ID&&f.Flags==old.Flags,"Binder identity changed");if(Key(f)!="ItemParam.param"&&Key(f)!="ItemTypeParam.param")Need(old.Item3.SequenceEqual(f.Bytes),"Unrelated binder entry changed: "+f.Name);}
   var ci=PARAM.Read(check.Files.Single(f=>Key(f)=="ItemParam.param").Bytes);ci.ApplyParamdef(itemDef);Validate(ci,bi,true);
   var ct=PARAM.Read(check.Files.Single(f=>Key(f)=="ItemTypeParam.param").Bytes);ct.ApplyParamdef(typeDef);Validate(ct,bt,false);
   var cr=ct.Rows.Single(r=>r.ID==Lantern);
   for(int i=0;i<cr.Cells.Count;i++)Need(Value(cr.Cells[i].Value)==(cr.Cells[i].Def.InternalName is "Unk10" or "Unk14"?"20":bt[260][i]),"Owned type differs beyond animation");
   Need(ci.Rows.Count(r=>Convert.ToInt32(r["itemTypeParamId"].Value)==Lantern)==1,"Owned type shared");
   report.Add(new{Path=x.Key,Before=x.Value,After=Hash(File.ReadAllBytes(dst)),ItemTypeFrom=260,ItemTypeTo=Lantern,AnimationFrom=210,AnimationTo=20,Usage=2200,MotionSpeed=2.0});
  }
  var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Select(l=>{var key=l.Split('\t')[1];return Hash(File.ReadAllBytes(Path.Combine(output,"assets",key))).ToLowerInvariant()+"\t"+key;})));
  File.WriteAllText(Path.Combine(output,"jump-off.tsv"),Hash(File.ReadAllBytes(Path.Combine(output,"variants/standard-jump.bnd.dcx"))).ToLowerInvariant()+"\tvariants/standard-jump.bnd.dcx\n");
  File.WriteAllLines(Path.Combine(output,"estus-off.tsv"),new[]{"jump-on\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-on.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-on.bnd.dcx","jump-off\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-off.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-off.bnd.dcx"});
  File.AppendAllLines(Path.Combine(output,"forge.ini"),new[]{"LanternAnimationCandidate=repair-powder-index-20","LanternOwnedItemType=60425001"});
  var allowed=Inputs.Keys.Concat(new[]{"files.tsv","jump-off.tsv","estus-off.tsv","forge.ini"}).ToHashSet();foreach(var x in before)if(!allowed.Contains(x.Key))Need(Hash(File.ReadAllBytes(Path.Combine(output,x.Key)))==x.Value,"Unrelated pack file changed: "+x.Key);
  File.WriteAllText(Path.Combine(output,"lantern-animation.audit.json"),JsonSerializer.Serialize(new{Regulations=report,TypeSchemaHash=Hash(File.ReadAllBytes(typeSchema)),OriginalDarksignUntouched=true,OriginalRepairPowderUntouched=true,UsageAndConsumptionFlagsUnchanged=true,ActiveEventByteIdentical=true,EstusByteIdentical=true,CoreUnchanged=true,LiveValidated=false},new JsonSerializerOptions{WriteIndented=true}));
  Console.WriteLine("Four variants verified: owned Darksign type clone differs only in two animation indices; Lantern usage2200 retained. Candidate requires live activation/reuse proof.");
 }
}
