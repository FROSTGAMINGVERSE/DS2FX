using SoulsFormats;
using System.Security.Cryptography;
using System.Text.Json;
static class SplitLanternPack {
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
 static string Key(BinderFile f)=>Path.GetFileName(f.Name.Replace('\\','/'));
 static object Copy(object v)=>v is byte[] b?b.ToArray():v;
 static bool Same(object a,object b)=>a is byte[] x&&b is byte[] y?x.SequenceEqual(y):Equals(a,b);
 static void Need(bool v,string s){if(!v)throw new InvalidDataException(s);}
 public static void Build(string modern,string lab,string vanilla,string schemas,string texts,string output){
 Need(!Directory.Exists(output),"Fresh output required");Directory.CreateDirectory(output);
 var defs=Directory.GetFiles(schemas,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(d=>d.ParamType);
 var manifest=new List<string>{"DS2FX-CONTENT-2 wayfarers-lantern 1.0.0 shared-content-experimental"};var compat=new List<string>{"DS2FX-INTEGRATION-1 wayfarers-lantern"};var audit=new List<object>();
 void WriteAsset(string key,byte[] bytes){var path=Path.Combine(output,"assets",key);Directory.CreateDirectory(Path.GetDirectoryName(path)!);File.WriteAllBytes(path,bytes);manifest.Add(Hash(bytes)+"\t"+key);}
 var old=BND4.Read(Path.Combine(modern,"assets/enc_regulation.bnd.dcx"));var current=BND4.Read(Path.Combine(lab,"assets/enc_regulation.bnd.dcx"));var baseReg=BND4.Read(vanilla);
 foreach(var e in current.Files){var o=old.Files.Single(f=>Key(f)==Key(e));if(e.Bytes.SequenceEqual(o.Bytes)||Key(e).StartsWith("ItemLotParam")||Key(e)=="PlayerStatusParam.param")continue;
 if(Key(e)=="SpEffectActiveItem.emevd"){
 var active=EMEVD.Read(e.Bytes);var originalActive=EMEVD.Read(o.Bytes);var targetActiveEntry=baseReg.Files.Single(x=>Key(x)==Key(e));var targetActive=EMEVD.Read(targetActiveEntry.Bytes);
 foreach(var ev in active.Events.Where(x=>x.ID==60425001)){Need(!originalActive.Events.Any(x=>x.ID==ev.ID)&&!targetActive.Events.Any(x=>x.ID==ev.ID),"Active-event ID collision");targetActive.Events.Add(ev);}
 targetActiveEntry.Bytes=targetActive.Write();audit.Add(new{Entry=Key(e),OwnedEventsAdded=true});continue;}
 Need(Key(e).EndsWith(".param"),"Unsupported changed regulation entry: "+Key(e));
 var p=PARAM.Read(e.Bytes);var prior=PARAM.Read(o.Bytes);var targetEntry=baseReg.Files.Single(f=>Key(f)==Key(e));var target=PARAM.Read(targetEntry.Bytes);var def=defs[p.ParamType];p.ApplyParamdef(def);prior.ApplyParamdef(def);target.ApplyParamdef(def);
 var report=new List<object>();
 foreach(var row in p.Rows){if(row.ID==60425000)continue;var before=prior.Rows.SingleOrDefault(r=>r.ID==row.ID);var dst=target.Rows.SingleOrDefault(r=>r.ID==row.ID);
 if(before==null){Need(dst==null,"Vanilla row collision: "+row.ID);dst=new PARAM.Row(row.ID,row.Name,def);for(int i=0;i<row.Cells.Count;i++)dst.Cells[i].Value=Copy(row.Cells[i].Value);target.Rows.Add(dst);report.Add(new{Row=row.ID,Added=true});continue;}
 for(int i=0;i<row.Cells.Count;i++)if(!Same(before.Cells[i].Value,row.Cells[i].Value)){Need(dst!=null,"Vanilla row missing: "+row.ID);dst!.Cells[i].Value=Copy(row.Cells[i].Value);report.Add(new{Row=row.ID,Field=row.Cells[i].Def.InternalName,Before=before.Cells[i].Value,After=row.Cells[i].Value});}
 }
 Need(prior.Rows.All(r=>p.Rows.Any(x=>x.ID==r.ID)),"Unexpected deleted regulation rows");target.Rows=target.Rows.OrderBy(r=>r.ID).ToList();targetEntry.Bytes=target.Write();
 var reread=PARAM.Read(targetEntry.Bytes);reread.ApplyParamdef(def);Need(reread.Rows.Count==target.Rows.Count,"Standalone roundtrip row count");foreach(var row in target.Rows){var r=reread.Rows.Single(x=>x.ID==row.ID);for(int i=0;i<row.Cells.Count;i++)Need(Same(row.Cells[i].Value,r.Cells[i].Value),"Standalone roundtrip field changed");}
 audit.Add(new{Entry=Key(e),Changes=report});
 }
 WriteAsset("enc_regulation.bnd.dcx",baseReg.Write());
 foreach(var line in File.ReadAllLines(Path.Combine(lab,"files.tsv")).Skip(1)){var x=line.Split('\t');var key=x[1];if(key=="enc_regulation.bnd.dcx")continue;var bytes=File.ReadAllBytes(Path.Combine(lab,"assets",key));Need(Hash(bytes)==x[0],"Lab asset hash mismatch");var oldPath=Path.Combine(modern,"assets",key);if(File.Exists(oldPath)&&File.ReadAllBytes(oldPath).SequenceEqual(bytes))continue;
 if(key.EndsWith(".fmg")){var before=FMG.Read(File.ReadAllBytes(oldPath));var now=FMG.Read(bytes);var baseline=FMG.Read(File.ReadAllBytes(Path.Combine(texts,Path.GetFileName(key))));foreach(var r in now.Entries){var o=before.Entries.SingleOrDefault(e=>e.ID==r.ID);if(o!=null&&o.Text==r.Text)continue;var dst=baseline.Entries.SingleOrDefault(e=>e.ID==r.ID);if(dst==null)baseline.Entries.Add(new FMG.Entry(r.ID,r.Text));else dst.Text=r.Text;}baseline.Entries=baseline.Entries.OrderBy(e=>e.ID).ToList();WriteAsset(key,baseline.Write());
 var rel="integration/modern-souls-2/"+key;var path=Path.Combine(output,rel);Directory.CreateDirectory(Path.GetDirectoryName(path)!);File.WriteAllBytes(path,bytes);compat.Add("modern-souls-2\t"+key+"\t"+Hash(File.ReadAllBytes(oldPath))+"\t"+Hash(bytes)+"\t"+rel);
 }else WriteAsset(key,bytes);
 }
 foreach(var rel in new[]{"assets/enc_regulation.bnd.dcx","variants/standard-jump.bnd.dcx","variants/estus-off-jump-on.bnd.dcx","variants/estus-off-jump-off.bnd.dcx"}){var bytes=File.ReadAllBytes(Path.Combine(lab,rel));var dest="integration/modern-souls-2/"+rel;var path=Path.Combine(output,dest);Directory.CreateDirectory(Path.GetDirectoryName(path)!);File.WriteAllBytes(path,bytes);compat.Add("modern-souls-2\tenc_regulation.bnd.dcx\t"+Hash(File.ReadAllBytes(Path.Combine(modern,rel)))+"\t"+Hash(bytes)+"\t"+dest);}
 File.WriteAllLines(Path.Combine(output,"files.tsv"),manifest);File.WriteAllLines(Path.Combine(output,"compatibility.tsv"),compat);File.WriteAllText(Path.Combine(output,"forge.ini"),"[Mod]\nName=Wayfarer's Lantern\nDescription=Reusable hip lantern; bonfire delivery, wick flame and anchored light. Requires DS2FX Core 0.5.0.\nCredits=FROST FX\nRequiresCore=0.5.0-alpha.1\n[LanternFixture]\nLanternDelivery=ds2fx-bonfire-missing-item\nLanternPickupLot=none\nLanternStarterGrant=false\n");File.WriteAllText(Path.Combine(output,"split.audit.json"),JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine("Standalone Lantern and four Modern Souls 2 integrations built; assets="+(manifest.Count-1));
 }
}
