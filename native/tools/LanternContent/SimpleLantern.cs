using SoulsFormats;
using System.Security.Cryptography;
using System.Text.Json;
static class SimpleLantern
{
 static string Hash(string p)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(p)));
 static void Need(bool ok,string msg){if(!ok)throw new InvalidDataException(msg);}
 public static void Build(string input,string output){
  input=Path.GetFullPath(input);output=Path.GetFullPath(output);
  Need(!Directory.Exists(output)&&!output.StartsWith(input,StringComparison.OrdinalIgnoreCase),"Fresh external candidate required");
  var regulations=new Dictionary<string,string>{
   ["assets/enc_regulation.bnd.dcx"]="94BE28F184793F9B5B17EC3E01753E5DCA9F49CF1DF8CB159F099603771E326C",
   ["variants/standard-jump.bnd.dcx"]="AE16A3AA5BFFDEBACAA4CE4B7D23B9C13BBB4AA0CDC7B7BC4EC02A41983A4130",
   ["variants/estus-off-jump-on.bnd.dcx"]="BBEDABE07B5479C8743B9B2EFA291300A0DA716098DEBC0B5C6F1C528DD2BC83",
   ["variants/estus-off-jump-off.bnd.dcx"]="7D8A8B755DD007297CA1C93605A930AA73D0DC97E2244442400F79DC156C3272"};
  foreach(var x in regulations)Need(Hash(Path.Combine(input,x.Key))==x.Value,"Unknown regulation: "+x.Key);
  var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(p=>Path.GetRelativePath(input,p).Replace('\\','/'),Hash);
  foreach(var p in before.Keys){var dst=Path.Combine(output,p);Directory.CreateDirectory(Path.GetDirectoryName(dst)!);File.Copy(Path.Combine(input,p),dst);}
  var changed=new HashSet<string>{"files.tsv","forge.ini"};
  foreach(var name in new[]{"simpleexplanation.fmg","detailedexplanation.fmg"}){
   var rel="assets/menu/text/english/"+name;var file=Path.Combine(output,rel);var f=FMG.Read(file);
   var original=f.Entries.ToDictionary(e=>e.ID,e=>e.Text);var lantern=f.Entries.Single(e=>e.ID==60425001);
   lantern.Text=name=="simpleexplanation.fmg"?"Reusable wearable lantern. Use to light or stow.":"A reusable lantern worn at the hip.\n\nUse to show the lit lantern or stow it.\nIts flame supplies your character light while visible.\nStowing extinguishes the flame and character light.";
   f.Write(file);var verify=FMG.Read(file);Need(verify.Entries.Count==original.Count,"FMG count changed");
   foreach(var e in verify.Entries)Need(e.Text==(e.ID==60425001?lantern.Text:original[e.ID]),"Unrelated text changed");changed.Add(rel);
  }
  const string retired="assets/menu/tex/icon/ic_0060425002.tpf";
  Need(before.ContainsKey(retired)&&before[retired]=="BE4F7F4EB5D26243C4883A1C4DD7E32518334D3DCEE4D5D117500F830464F0E2","Missing owned depleted icon");
  // Hash is recorded in audit and staging manifest; removal is limited to this owned icon.
  File.Delete(Path.Combine(output,retired));changed.Add(retired);
  var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);
  File.WriteAllLines(manifest,new[]{lines[0]}.Concat(lines.Skip(1).Where(l=>l.Split('\t')[1]!="menu/tex/icon/ic_0060425002.tpf").Select(l=>{var key=l.Split('\t')[1];return Hash(Path.Combine(output,"assets",key)).ToLowerInvariant()+"\t"+key;})));
  var ini=Path.Combine(output,"forge.ini");var obsolete=new[]{"LanternResource=","LanternCapacity=","ChargesPersistence=","LanternFlameDriver=","LanternChargeDisplay=","LanternDepletedIcon=","NativeLightMode=","NativeLanternMode="};
  File.WriteAllLines(ini,File.ReadAllLines(ini).Where(l=>!obsolete.Any(k=>l.StartsWith(k))).Concat(new[]{"LanternResource=unlimited-reusable-toggle","NativeLanternMode=authenticated-use-toggle","NativeLightMode=lantern-owns-canonical-character-light","LanternFlameDriver=toggle-native-sfx-exact-handle","LanternChargeDisplay=none"}));
  foreach(var x in before)if(!changed.Contains(x.Key))Need(Hash(Path.Combine(output,x.Key))==x.Value,"Unrelated content changed: "+x.Key);
  foreach(var x in regulations)Need(Hash(Path.Combine(output,x.Key))==x.Value,"Regulation changed");
  File.WriteAllText(Path.Combine(output,"simple-lantern.audit.json"),JsonSerializer.Serialize(new{Regulations=regulations,Changed=changed,RemovedIconHash=before[retired],EstusAndItemsUnchanged=true,NoCharges=true,NoHud=true,LiveValidated=false},new JsonSerializerOptions{WriteIndented=true}));
  Console.WriteLine("Simple toggle content built; all four regulations byte-identical; only Lantern text and owned presentation metadata changed.");
 }
}
