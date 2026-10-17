using SoulsFormats;
using System.Security.Cryptography;
using System.Text.Json;
using System.Globalization;
using System.Text.RegularExpressions;

System.Text.Encoding.RegisterProvider(System.Text.CodePagesEncodingProvider.Instance);
if (args.Length != 5) throw new ArgumentException("ModernEnemyTuning <Modern pack> <optional installed Lantern pack> <vanilla binder> <defs> <fresh output>");
const double RetainedIncrease = 0.4;
void Need(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
string Hash(byte[] data) => Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
string Key(BinderFile f) => Path.GetFileName(f.Name.Replace('\\', '/'));
string Value(object v) => v is byte[] b ? Convert.ToHexString(b) : Convert.ToString(v, CultureInfo.InvariantCulture)!;
var modern = Path.GetFullPath(args[0]); var lantern = Path.GetFullPath(args[1]); var output = Path.GetFullPath(args[4]);
Need(!Directory.Exists(output) && !output.StartsWith(modern + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) && !output.StartsWith(lantern + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Fresh external output required");
Need(Hash(File.ReadAllBytes(args[2])) == "97a2b95310d5e0ded117435d242ca62e45ee5ba2901a73a92883a8f0431a991e", "Unknown vanilla baseline");
var relatives = new[] { "assets/enc_regulation.bnd.dcx", "variants/standard-jump.bnd.dcx", "variants/estus-off-jump-on.bnd.dcx", "variants/estus-off-jump-off.bnd.dcx" };
var modernHashes = new[] { "6f9194cc1e6c439cb5502da17c4ac2f8ffacc5123ee9e6043482179936a3e2e9", "619a0fdd266f2677179460204d875c9ef7dd62af0ba0c70bc2d7511f66e167d3", "9100ba01d37de0804caa89eb8dce65580bfb5e69f442eb0b5e525b68196e7391", "eb402c6b1f1f638214493386b080f248f14c054318625ae6935fe65a31b254bd" };
var lanternHashes = new[] { "c0dc7544790f5296eb05dcd8afe25ffde281ed86628c5dd7a2851b574b6a4288", "be3043dce7adc22a37194eeb4b92fb41307c61cdfd7610adb4614df135fcf52c", "232efb9992c7c0314764dd1d0c7d3eb5d52ccac1f2656d14e6eb6e911668de39", "b7b1b8bb20006a978cbc240e6dc3a0309236a3c75f7a40ff48dc449fbb8c9f83" };
var haveLantern = Directory.Exists(lantern);
for (int i = 0; i < relatives.Length; i++) { Need(Hash(File.ReadAllBytes(Path.Combine(modern, relatives[i]))) == modernHashes[i], "Unknown Modern input"); if (haveLantern) Need(Hash(File.ReadAllBytes(Path.Combine(lantern, "integration/modern-souls-2", relatives[i]))) == lanternHashes[i], "Unknown optional Lantern integration input"); }
var targets = new Dictionary<string,(PARAMDEF Def, string Allowed)> {
 ["EnemyBehaviorParam.param"] = (PARAMDEF.XmlDeserialize(Path.Combine(args[3], "BEHAVIOR_PARAM.xml")), @"^Group (?:[1-9]|10): Unk(?:34|38)$"),
 ["EnemyBehaviorSecondParam.param"] = (PARAMDEF.XmlDeserialize(Path.Combine(args[3], "BEHAVIOR_PARAM.xml")), @"^Group (?:[1-9]|10): Unk(?:34|38)$"),
 ["EnemyBehaviorThirdParam.param"] = (PARAMDEF.XmlDeserialize(Path.Combine(args[3], "BEHAVIOR_PARAM.xml")), @"^Group (?:[1-9]|10): Unk(?:34|38)$"),
 ["EnemyComboBehaviorParam.param"] = (PARAMDEF.XmlDeserialize(Path.Combine(args[3], "COMBO_BEHAVIOR_PARAM.xml")), @"^Group [1-7]: Unk(?:60|64|68|6C|70|74|78|7C)$")
};
PARAM Read(byte[] bytes, PARAMDEF def) { var p = PARAM.Read(bytes); p.ApplyParamdef(def); return p; }
var vanillaFiles = BND4.Read(args[2]).Files.ToDictionary(Key);
var baseline = targets.ToDictionary(t => t.Key, t => Read(vanillaFiles[t.Key].Bytes, t.Value.Def));
var reports = new List<object>();
string Tune(string sourcePath, string destPath, string label) {
 var binder = BND4.Read(sourcePath); var originals = binder.Files.ToDictionary(f => f.Name, f => (f.ID, f.Flags, Bytes: f.Bytes.ToArray()));
 var counts = new List<object>();
 foreach (var target in targets) {
  var entry = binder.Files.Single(f => Key(f) == target.Key); var p = Read(entry.Bytes, target.Value.Def);
  var baseRows = baseline[target.Key].Rows.ToDictionary(r => r.ID);
  Need(p.Rows.Count == baseRows.Count, "Enemy row count drift");
  var originalRows = p.Rows.ToDictionary(r => r.ID, r => (r.Name, Values: r.Cells.Select(c => Value(c.Value)).ToArray()));
  var expected = new Dictionary<(int Row, int Cell),float>(); int preservedDecreases = 0;
  foreach (var row in p.Rows) {
   var b = baseRows[row.ID];
   for (int i = 0; i < row.Cells.Count; i++) {
    var c = row.Cells[i]; if (Value(c.Value) == Value(b.Cells[i].Value)) continue;
    Need(Regex.IsMatch(c.Def.InternalName, target.Value.Allowed) && c.Value is float && b.Cells[i].Value is float, "Unexpected inherited enemy change: " + c.Def.InternalName);
    var current = (float)c.Value; var original = (float)b.Cells[i].Value;
    Need(float.IsFinite(current) && float.IsFinite(original) && original > 0 && current > 0, "Invalid timing coefficient");
    if (current <= original) { preservedDecreases++; continue; }
    var tuned = (float)(original + RetainedIncrease * ((double)current - original));
    Need(tuned > original && tuned < current, "Tuning does not retain a reduced increase"); expected.Add((row.ID, i), tuned); c.Value = tuned;
   }
  }
  Need(expected.Count > 0, "No inherited increases found"); entry.Bytes = p.Write();
  var check = Read(entry.Bytes, target.Value.Def); Need(check.Rows.Count == originalRows.Count, "Enemy rows changed");
  foreach (var row in check.Rows) { var before = originalRows[row.ID]; Need(row.Name == before.Name, "Enemy row name changed"); for (int i = 0; i < row.Cells.Count; i++) Need(Value(row.Cells[i].Value) == (expected.TryGetValue((row.ID,i), out var e) ? Value(e) : before.Values[i]), "Unexpected enemy field change"); }
  counts.Add(new { Table = target.Key, TunedFields = expected.Count, PreservedDecreasedFields = preservedDecreases, ChangedRows = expected.Keys.Select(k => k.Row).Distinct().Count() });
 }
 Directory.CreateDirectory(Path.GetDirectoryName(destPath)!); binder.Write(destPath); var reread = BND4.Read(destPath);
 Need(reread.Files.Count == originals.Count, "Binder entry count changed");
 foreach (var f in reread.Files) { Need(originals.TryGetValue(f.Name, out var old) && old.ID == f.ID && old.Flags == f.Flags, "Binder identity changed"); if (!targets.ContainsKey(Key(f))) Need(f.Bytes.SequenceEqual(old.Bytes), "Unrelated binder entry changed: " + Key(f)); else Need(f.Bytes.SequenceEqual(binder.Files.Single(e => e.Name == f.Name).Bytes), "Saved enemy entry differs"); }
 reports.Add(new { Label = label, Before = Hash(File.ReadAllBytes(sourcePath)), After = Hash(File.ReadAllBytes(destPath)), Counts = counts, OtherEntriesByteIdentical = true });
 return Hash(File.ReadAllBytes(destPath));
}
var modernOutMod = Path.Combine(output, "Modern Souls 2"); var modernOut = Path.Combine(modernOutMod, "Root/Game/DS2FX/Content/modern-souls-2");
var originalPackFiles = Directory.GetFiles(modern,"*",SearchOption.AllDirectories).ToDictionary(f => Path.GetRelativePath(modern, f), f => Hash(File.ReadAllBytes(f)));
foreach (var rel in originalPackFiles.Keys) { var dest = Path.Combine(modernOut, rel); Directory.CreateDirectory(Path.GetDirectoryName(dest)!); File.Copy(Path.Combine(modern, rel), dest); }
var newModernHashes = relatives.Select((rel,i) => Tune(Path.Combine(modern, rel), Path.Combine(modernOut, rel), "Modern/" + rel)).ToArray();
var manifest = File.ReadAllLines(Path.Combine(modern, "files.tsv")); Need(manifest[0] == "DS2FX-CONTENT-2 modern-souls-2 0.2.0 shared-content-experimental", "Unknown Modern manifest");
File.WriteAllLines(Path.Combine(modernOut, "files.tsv"), new[] { "DS2FX-CONTENT-2 modern-souls-2 0.2.1 shared-content-experimental" }.Concat(manifest.Skip(1).Select(line => { var f=line.Split('\t'); return Hash(File.ReadAllBytes(Path.Combine(modernOut,"assets",f[1]))) + "\t" + f[1]; })));
File.WriteAllText(Path.Combine(modernOut, "jump-off.tsv"), newModernHashes[1] + "\t" + relatives[1] + "\n");
File.WriteAllLines(Path.Combine(modernOut, "estus-off.tsv"), new[] { "jump-on\t"+newModernHashes[2]+"\t"+relatives[2], "jump-off\t"+newModernHashes[3]+"\t"+relatives[3] });
var modifiedModern = relatives.Concat(new[] { "files.tsv", "jump-off.tsv", "estus-off.tsv" }).ToHashSet();
foreach (var pair in originalPackFiles) { Need(Hash(File.ReadAllBytes(Path.Combine(modern,pair.Key))) == pair.Value, "Modern source changed during build"); if (!modifiedModern.Contains(pair.Key.Replace('\\','/'))) Need(Hash(File.ReadAllBytes(Path.Combine(modernOut,pair.Key))) == pair.Value, "Unrelated Modern asset changed"); }
string? compatOut = null;
if (haveLantern) {
 compatOut = Path.Combine(output,"Optional Modern Souls 2 Lantern Compatibility/Root/Game/DS2FX/Content/wayfarers-lantern");
 var originalCompat = File.ReadAllLines(Path.Combine(lantern,"compatibility.tsv")); Need(originalCompat[0] == "DS2FX-INTEGRATION-1 wayfarers-lantern", "Unknown optional integration contract"); var records = originalCompat.ToList();
 for (int i=0;i<relatives.Length;i++) {
  var rel = "integration/modern-souls-2/enemy-tuning-0.2.1/"+relatives[i]; var dest = Path.Combine(compatOut,rel);
  var hash = Tune(Path.Combine(lantern,"integration/modern-souls-2",relatives[i]),dest,"Optional combined/"+relatives[i]);
  records.Add("modern-souls-2\tenc_regulation.bnd.dcx\t"+newModernHashes[i]+"\t"+hash+"\t"+rel);
  var m=BND4.Read(Path.Combine(modernOut,relatives[i])).Files.ToDictionary(Key); var l=BND4.Read(dest).Files.ToDictionary(Key);
  foreach(var target in targets.Keys) Need(m[target].Bytes.SequenceEqual(l[target].Bytes), "Combined enemy timing diverges");
 }
 File.WriteAllLines(Path.Combine(compatOut,"compatibility.tsv"),records);
 Need(File.ReadAllLines(Path.Combine(compatOut,"compatibility.tsv")).Take(originalCompat.Length).SequenceEqual(originalCompat),"Older optional routes removed");
}
File.WriteAllText(Path.Combine(modernOut,"forge.ini"),"[Mod]\nName=Modern Souls 2\nDescription=Player movement and combat improvements with a moderated enemy combat pace. Dedicated Jump and Walking Estus are configured through Forge.\nCredits=Based on Balanced DS3 Combat by Fentiman; DS2FX adaptations by FROST FX\nRequiresCore=0.4.1-alpha.1\n");
File.WriteAllText(Path.Combine(modernOutMod,"meta.ini"),"[General]\ngameName=DarkSoulsII\nmodid=0\nversion=0.2.1\ncomments=Requires DS2FX. Reduced inherited enemy animation/combo increases; independent of Wayfarer's Lantern.\n");
File.WriteAllText(Path.Combine(modernOutMod,"README.md"),"# Modern Souls 2 0.2.1\n\nRequires DS2FX Core0.4.1-alpha.1 or later. Works independently of Wayfarer's Lantern. Based on Balanced DS3 Combat by Fentiman; DS2FX adaptations by FROST FX.\n\nEnemy animation/combo increases have been moderated: each increased value retains40% of its previous increase above vanilla. The game has per-action settings; this is not a universal1.2x enemy-speed switch. Existing slowdowns, enemy HP/hitboxes and player improvements are retained.\n\nPress END to open Forge. Dedicated Jump and Walking Estus apply after restart. Configure jump on L3 in the game's controls. Co-op peers should use matching gameplay content/settings.\n\nAn optional DS2FX compatibility patch is supplied for installations that also use the Lantern, because both modify the same game regulation file. Neither mod requires the other. The patch retains older integration routes and adds four merged0.2.1 variants.\n\nStatic and launcher checks are separate from live combat timing and multiplayer validation. This local build contains game-derived content and is not a redistributable base archive.\n");
File.WriteAllText(Path.Combine(modernOutMod,"README.txt"),"Modern Souls 2 0.2.1\nRequires DS2FX only. Independent of Wayfarer's Lantern.\nPlayer movement/combat improvements with moderated enemy animation/combo increases.\nPress END for Dedicated Jump and Walking Estus; restart after changing settings.\nSee README.md for details, source credits and optional compatibility information.\n");
if(compatOut!=null)File.WriteAllText(Path.Combine(output,"Optional Modern Souls 2 Lantern Compatibility/README.md"),"# Optional DS2FX compatibility data\n\nUse only when Modern Souls 2 0.2.1 and Wayfarer's Lantern1.0.1 are installed together. Each mod independently requires only DS2FX. This optional overlay adds four audited combined regulation variants to the existing Lantern content directory and updates its compatibility manifest. Standalone Lantern content and native modules are unchanged; older integration routes remain supported.\n");
File.WriteAllText(Path.Combine(output,"enemy-tuning.audit.json"),JsonSerializer.Serialize(new { Version="0.2.1",RetainedIncrease,Formula="vanilla + 0.4 * (current - vanilla), increases only",Reports=reports,ModernRequiresOnlyDS2FX=true,LanternStandaloneUntouched=true,OptionalCompatibility=compatOut,OldCompatibilityRoutesRetained=true,EnemyHPAndHitboxesUnchanged=true,PlayerChangesUnchanged=true,NativeChanges=false,LiveValidated=false },new JsonSerializerOptions {WriteIndented=true}));
Console.WriteLine("PASS: enemy increases reduced to40% of the original increase in four Modern variants; optional combined routes generated; standalone Lantern untouched; all unrelated entries preserved.");
