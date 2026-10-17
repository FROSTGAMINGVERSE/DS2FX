using SoulsFormats;
using System.Security.Cryptography;
using System.Text.Json;
using System.Globalization;

// Retire owned acquisition references. Native DS2FX now grants the missing
// Lantern at an authenticated local bonfire; no save or quick-slot mutation.
static class LanternBonfireDelivery
{
 const int Lantern=60425001;
 static readonly Dictionary<string,string> Inputs=new(){
  ["assets/enc_regulation.bnd.dcx"]="280C96DFBEFA75F3CF79A9CA7BF0FF7F53E507F0C627E62DDA7B42DEFE27BC26",
  ["variants/standard-jump.bnd.dcx"]="63F603208CCB9208F8790E53C16FB2C3D3922E754975428A7503570CCB937748",
  ["variants/estus-off-jump-on.bnd.dcx"]="5C3D44095EAAA7912E4027F19A5D6FA38614AC03F0E52348F3985316770F96D7",
  ["variants/estus-off-jump-off.bnd.dcx"]="ABD0407ADDD2245DA5A0B12FC87CECDAEC6010C60779667DE4BDD840F1C7E34D"};
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static string Key(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
 static string Value(object v)=>v is byte[] b?Convert.ToHexString(b):Convert.ToString(v,CultureInfo.InvariantCulture)!;
 static void Need(bool ok,string m){if(!ok)throw new InvalidDataException(m);}
 static void Set(PARAM.Row r,string field,int v)=>r[field].Value=Convert.ChangeType(v,r[field].Value.GetType());
 public static void Build(string input,string schemas,string output){
  input=Path.GetFullPath(input);output=Path.GetFullPath(output);
  Need(!Directory.Exists(output)&&!output.Equals(input,StringComparison.OrdinalIgnoreCase)&&!output.StartsWith(input+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Fresh external output required");
  foreach(var x in Inputs)Need(Hash(File.ReadAllBytes(Path.Combine(input,x.Key)))==x.Value,"Unknown regulation: "+x.Key);
  var defs=Directory.GetFiles(schemas,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(d=>d.ParamType);
  var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(p=>Path.GetRelativePath(input,p).Replace('\\','/'),p=>Hash(File.ReadAllBytes(p)));
  foreach(var rel in before.Keys){var dst=Path.Combine(output,rel);Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.Copy(Path.Combine(input,rel),dst);}
  var report=new List<object>();
  foreach(var x in Inputs){
   var binder=BND4.Read(Path.Combine(input,x.Key));
   var originals=binder.Files.Select(f=>(f.Name,f.ID,f.Flags,f.CompressionType,f.Bytes.ToArray())).ToArray();
   var edits=new List<object>();
   foreach(var entry in binder.Files.Where(f=>Key(f)=="PlayerStatusParam.param"||Key(f)=="ShopLineupParam.param"||Key(f).StartsWith("ItemLotParam2_")).ToArray()){
    var p=PARAM.Read(entry.Bytes);p.ApplyParamdef(defs[p.ParamType]);
    var baseline=p.Rows.ToDictionary(r=>r.ID,r=>(r.Name,Values:r.Cells.Select(c=>Value(c.Value)).ToArray()));
    var changed=new Dictionary<int,HashSet<string>>();var removed=new HashSet<int>();
    foreach(var row in p.Rows.ToArray()){
     var fields=new HashSet<string>();
     if(Key(entry)=="PlayerStatusParam.param"){
      for(int n=1;n<=10;n++)if(Convert.ToInt32(row[$"beltItem{n:D2}"].Value)==Lantern){
       foreach(var field in new[]{$"beltItem{n:D2}",$"beltItem{n:D2}Num"}){Set(row,field,field.EndsWith("Num")?0:-1);fields.Add(field);}
      }
     }else if(Key(entry)=="ShopLineupParam.param"&&Convert.ToInt32(row["saleItem"].Value)==Lantern){p.Rows.Remove(row);removed.Add(row.ID);}
     else if(Key(entry).StartsWith("ItemLotParam2_")){
      for(int n=1;n<=10;n++)if(Convert.ToInt32(row[$"itemId{n}"].Value)==Lantern){
       Need(Key(entry)=="ItemLotParam2_Other.param"&&row.ID==10026050,"Unexpected Lantern loot owner; inspect before removal");
       Need(Convert.ToInt32(row["itemId1"].Value)==60630000&&Convert.ToInt32(row["itemId2"].Value)==60420000,"Original soul/Torch identity changed");
       foreach(var field in new[]{$"itemId{n}",$"itemNum{n}",$"itemWeight{n}"}){Set(row,field,0);fields.Add(field);}
      }
     }
     if(fields.Count>0)changed[row.ID]=fields;
    }
    if(changed.Count==0&&removed.Count==0)continue;
    entry.Bytes=p.Write();var verify=PARAM.Read(entry.Bytes);verify.ApplyParamdef(defs[verify.ParamType]);
    Need(verify.Rows.Count==baseline.Count-removed.Count,"Unexpected PARAM row count");
    foreach(var r in verify.Rows){Need(baseline.ContainsKey(r.ID)&&r.Name==baseline[r.ID].Name,"PARAM identity changed");
     for(int i=0;i<r.Cells.Count;i++)Need(Value(r.Cells[i].Value)==baseline[r.ID].Values[i]||(changed.TryGetValue(r.ID,out var fields)&&fields.Contains(r.Cells[i].Def.InternalName)),"Unrelated field changed: "+r.ID+"/"+r.Cells[i].Def.InternalName);
     Need(r.Cells.Select(c=>Value(c.Value)).SequenceEqual(p.Rows.Single(z=>z.ID==r.ID).Cells.Select(c=>Value(c.Value))),"PARAM serialization changed requested field");
    }
    edits.Add(new{Entry=Key(entry),ChangedFields=changed.ToDictionary(z=>z.Key,z=>z.Value.ToArray()),RemovedLanternShopRows=removed.ToArray()});
   }
   var dst=Path.Combine(output,x.Key);binder.Write(dst);var check=BND4.Read(dst);
   Need(check.Files.Count==originals.Length,"Binder count changed");
   for(int i=0;i<check.Files.Count;i++){
    var f=check.Files[i];var old=originals[i];Need(f.Name==old.Name&&f.ID==old.ID&&f.Flags==old.Flags&&f.CompressionType==old.CompressionType,"Binder identity/order changed");
    Need(f.Bytes.SequenceEqual(binder.Files[i].Bytes),"Binder roundtrip mismatch");
    if(Key(f)!="PlayerStatusParam.param"&&Key(f)!="ShopLineupParam.param"&&!Key(f).StartsWith("ItemLotParam2_"))Need(f.Bytes.SequenceEqual(old.Item5),"Unrelated binder payload changed");
    if(Key(f)=="PlayerStatusParam.param"||Key(f)=="ShopLineupParam.param"||Key(f).StartsWith("ItemLotParam2_")){
     var p=PARAM.Read(f.Bytes);p.ApplyParamdef(defs[p.ParamType]);
     foreach(var r in p.Rows){
      if(Key(f)=="PlayerStatusParam.param")Need(Enumerable.Range(1,10).All(n=>Convert.ToInt32(r[$"beltItem{n:D2}"].Value)!=Lantern),"Starter equip remains");
      else if(Key(f)=="ShopLineupParam.param")Need(Convert.ToInt32(r["saleItem"].Value)!=Lantern,"Shop grant remains");
      else Need(Enumerable.Range(1,10).All(n=>Convert.ToInt32(r[$"itemId{n}"].Value)!=Lantern),"Corpse grant remains");
     }
    }
   }
   report.Add(new{Path=x.Key,Before=x.Value,After=Hash(File.ReadAllBytes(dst)),Changes=edits});
  }
  var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);
  File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Select(l=>{var key=l.Split('\t')[1];return Hash(File.ReadAllBytes(Path.Combine(output,"assets",key))).ToLowerInvariant()+"\t"+key;})));
  File.WriteAllText(Path.Combine(output,"jump-off.tsv"),Hash(File.ReadAllBytes(Path.Combine(output,"variants/standard-jump.bnd.dcx"))).ToLowerInvariant()+"\tvariants/standard-jump.bnd.dcx\n");
  File.WriteAllLines(Path.Combine(output,"estus-off.tsv"),new[]{"jump-on\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-on.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-on.bnd.dcx","jump-off\t"+Hash(File.ReadAllBytes(Path.Combine(output,"variants/estus-off-jump-off.bnd.dcx"))).ToLowerInvariant()+"\tvariants/estus-off-jump-off.bnd.dcx"});
  var metadata=Path.Combine(output,"forge.ini");var text=File.ReadAllText(metadata);
  text=System.Text.RegularExpressions.Regex.Replace(text,@"(?m)^Role=[^\r\n]*","Role=bonfire-missing-item-delivery");
  text=System.Text.RegularExpressions.Regex.Replace(text,@"(?m)^LanternPickupLot=[^\r\n]*","LanternPickupLot=none");
  text=System.Text.RegularExpressions.Regex.Replace(text,@"(?m)^LanternPickupLocation=[^\r\n]*","LanternPickupLocation=none");
  File.WriteAllText(metadata,text.TrimEnd()+"\nLanternDelivery=ds2fx-bonfire-missing-item\nLanternAutoEquip=false\n");
  var allowed=Inputs.Keys.Concat(new[]{"files.tsv","jump-off.tsv","estus-off.tsv","forge.ini"}).ToHashSet();
  foreach(var x in before)if(!allowed.Contains(x.Key))Need(Hash(File.ReadAllBytes(Path.Combine(output,x.Key)))==x.Value,"Unrelated pack file changed: "+x.Key);
  File.WriteAllText(Path.Combine(output,"lantern-bonfire-delivery.audit.json"),JsonSerializer.Serialize(new{Regulations=report,NativeDeliveryItem=Lantern,CorpseSoulAndTorchPreserved=true,ItemAndTypeAndUsageAndActiveEventByteIdentical=true,OilReintroduced=false,AutomaticEquip=false,LiveValidated=false},new JsonSerializerOptions{WriteIndented=true}));
  Console.WriteLine("Four variants verified: Lantern removed from corpse/starter/shop routes; original Torch/soul and all usable-item behavior preserved.");
 }
}
