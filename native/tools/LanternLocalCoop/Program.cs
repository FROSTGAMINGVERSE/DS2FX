using SoulsFormats;
using System.Globalization;
using System.Security.Cryptography;
using System.Text.Json;

// Owned Lantern-only use permission patch. Never writes the supplied input pack.
System.Text.Encoding.RegisterProvider(System.Text.CodePagesEncodingProvider.Instance);
if (args.Length != 3) throw new ArgumentException("LanternLocalCoop <installed content pack> <Scholar defs> <fresh candidate pack>");
const int Lantern = 60425001, DarksignUsage = 2200;
var input = Path.GetFullPath(args[0]); var output = Path.GetFullPath(args[2]);
void Need(bool ok, string error) { if (!ok) throw new InvalidDataException(error); }
Need(!Directory.Exists(output) && !output.StartsWith(input + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Fresh external output required");
string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
string Value(object value) => value is byte[] bytes ? Convert.ToHexString(bytes) : Convert.ToString(value, CultureInfo.InvariantCulture)!;
string Key(BinderFile file) => Path.GetFileName(file.Name.Replace('\\', '/'));
var pinned = new Dictionary<string,string> {
 ["assets/enc_regulation.bnd.dcx"] = "25543f4f10a2b06f00fe9507a129a4b3406c6d75a8a274b9abb55e7ac3f5ce3a",
 ["integration/modern-souls-2/assets/enc_regulation.bnd.dcx"] = "f24926fa6a831a3c87b99f44a89f620c46c4886785c400d0e341936c6d464fd5",
 ["integration/modern-souls-2/variants/standard-jump.bnd.dcx"] = "5146372a1df0d3421c1c47950e722e63b605553d083c5568e22ad9522fecf7c4",
 ["integration/modern-souls-2/variants/estus-off-jump-on.bnd.dcx"] = "3d1dd9564a5cfecd63825cf60d2e24ac07e93391e1d42d585a433973b7f18b55",
 ["integration/modern-souls-2/variants/estus-off-jump-off.bnd.dcx"] = "3c7df334ba2d7747bdce94fc4b898e7e0b0f7e8ce00ce17550c58dd202e83ddb"
};
var itemDef = PARAMDEF.XmlDeserialize(Path.Combine(args[1], "ITEM_PARAM.xml"));
var usageDef = PARAMDEF.XmlDeserialize(Path.Combine(args[1], "ITEM_USAGE_PARAM.xml"));
Need(usageDef.ParamType == "ITEM_USAGE_PARAM" && usageDef.DataVersion == 8, "Unexpected usage schema");
foreach (var pair in pinned) Need(Hash(File.ReadAllBytes(Path.Combine(input, pair.Key))) == pair.Value, "Unknown input: " + pair.Key);
var originalFiles = Directory.GetFiles(input, "*", SearchOption.AllDirectories).ToDictionary(p => Path.GetRelativePath(input, p).Replace('\\', '/'), p => Hash(File.ReadAllBytes(p)));
var firstManifest = File.ReadAllLines(Path.Combine(input, "files.tsv"));
Need(firstManifest[0] == "DS2FX-CONTENT-2 wayfarers-lantern 1.0.0 shared-content-experimental", "Unknown manifest version");
foreach (var relative in originalFiles.Keys) { var dest = Path.Combine(output, relative); Directory.CreateDirectory(Path.GetDirectoryName(dest)!); File.Copy(Path.Combine(input, relative), dest); }
var reports = new List<object>();
foreach (var pair in pinned) {
 var binder = BND4.Read(Path.Combine(input, pair.Key));
 var originals = binder.Files.ToDictionary(f => f.Name, f => (f.ID, f.Flags, Bytes: f.Bytes.ToArray()));
 var itemEntry = binder.Files.Single(f => Key(f) == "ItemParam.param");
 var usageEntry = binder.Files.Single(f => Key(f) == "ItemUsageParam.param");
 PARAM Read(byte[] bytes, PARAMDEF def) { var p = PARAM.Read(bytes); p.ApplyParamdef(def); return p; }
 var items = Read(itemEntry.Bytes, itemDef); var usages = Read(usageEntry.Bytes, usageDef);
 var beforeItems = items.Rows.ToDictionary(r => r.ID, r => (r.Name, Cells: r.Cells.ToDictionary(c => c.Def.InternalName, c => Value(c.Value))));
 var beforeUsages = usages.Rows.ToDictionary(r => r.ID, r => (r.Name, Cells: r.Cells.ToDictionary(c => c.Def.InternalName, c => Value(c.Value))));
 var item = items.Rows.Single(r => r.ID == Lantern); var source = usages.Rows.Single(r => r.ID == DarksignUsage);
 Need(Convert.ToInt32(item["itemTypeParamId"].Value) == Lantern && Convert.ToInt32(item["itemUsageParamId"].Value) == DarksignUsage && Convert.ToInt32(item["maxNum"].Value) == 1 && Convert.ToInt32(item["specialEffectId"].Value) == Lantern, "Unexpected Lantern item contract");
 Need(!usages.Rows.Any(r => r.ID == Lantern) && !items.Rows.Any(r => Convert.ToInt32(r["itemUsageParamId"].Value) == Lantern), "Owned usage ID collision");
 Need(Convert.ToInt32(source["isConsume"].Value) == 0 && Convert.ToInt32(source["permitOnlineMulti"].Value) == 0 && Convert.ToInt32(source["permitOnlineGuest"].Value) == 0 && Convert.ToInt32(source["exEvalFuncID"].Value) == 0, "Unexpected Darksign permissions");
 var owned = new PARAM.Row(Lantern, "DS2FX Lantern - reusable local co-op use", usageDef);
 for (int i = 0; i < source.Cells.Count; i++) owned.Cells[i].Value = source.Cells[i].Value is byte[] b ? b.ToArray() : source.Cells[i].Value;
 owned["permitOnlineMulti"].Value = (byte)1; owned["permitOnlineGuest"].Value = (byte)1;
 usages.Rows.Add(owned); usages.Rows.Sort((a,b) => a.ID.CompareTo(b.ID)); item["itemUsageParamId"].Value = Lantern;
 itemEntry.Bytes = items.Write(); usageEntry.Bytes = usages.Write(); binder.Write(Path.Combine(output, pair.Key));
 var check = BND4.Read(Path.Combine(output, pair.Key)); Need(check.Files.Count == originals.Count, "Binder entry count changed");
 foreach (var f in check.Files) { Need(originals.TryGetValue(f.Name, out var old) && old.ID == f.ID && old.Flags == f.Flags, "Binder identity changed"); if (Key(f) is not "ItemParam.param" and not "ItemUsageParam.param") Need(old.Bytes.SequenceEqual(f.Bytes), "Unrelated entry changed: " + f.Name); }
 var ci = Read(check.Files.Single(f => Key(f) == "ItemParam.param").Bytes, itemDef);
 var cu = Read(check.Files.Single(f => Key(f) == "ItemUsageParam.param").Bytes, usageDef);
 Need(ci.Rows.Count == beforeItems.Count && cu.Rows.Count == beforeUsages.Count + 1, "Unexpected row count");
 foreach (var r in ci.Rows) { var old = beforeItems[r.ID]; Need(old.Name == r.Name, "Item row renamed"); foreach (var c in r.Cells) Need(Value(c.Value) == (r.ID == Lantern && c.Def.InternalName == "itemUsageParamId" ? Lantern.ToString() : old.Cells[c.Def.InternalName]), "Unexpected item change: " + r.ID + "/" + c.Def.InternalName); }
 foreach (var r in cu.Rows.Where(r => r.ID != Lantern)) { var old = beforeUsages[r.ID]; Need(old.Name == r.Name, "Usage row renamed"); foreach (var c in r.Cells) Need(Value(c.Value) == old.Cells[c.Def.InternalName], "Existing usage changed: " + r.ID + "/" + c.Def.InternalName); }
 var verified = cu.Rows.Single(r => r.ID == Lantern);
 foreach (var c in verified.Cells) Need(Value(c.Value) == (c.Def.InternalName is "permitOnlineMulti" or "permitOnlineGuest" ? "1" : beforeUsages[DarksignUsage].Cells[c.Def.InternalName]), "Unexpected owned usage field");
 Need(ci.Rows.Count(r => Convert.ToInt32(r["itemUsageParamId"].Value) == Lantern) == 1, "Owned usage shared");
 reports.Add(new { Path = pair.Key, Before = pair.Value, After = Hash(File.ReadAllBytes(Path.Combine(output, pair.Key))), ExistingItemFieldsChanged = 1, ExistingUsageFieldsChanged = 0, AddedUsageRows = 1, Multiplayer = 1, Guest = 1, Consumed = 0, OriginalDarksignPreserved = true, OtherBinderEntriesByteIdentical = true });
}
File.WriteAllLines(Path.Combine(output, "files.tsv"), new[] { "DS2FX-CONTENT-2 wayfarers-lantern 1.0.1 shared-content-experimental" }.Concat(firstManifest.Skip(1).Select(line => { var fields = line.Split('\t'); Need(fields.Length == 2, "Malformed file manifest"); return Hash(File.ReadAllBytes(Path.Combine(output, "assets", fields[1]))) + "\t" + fields[1]; })));
File.WriteAllLines(Path.Combine(output, "compatibility.tsv"), File.ReadAllLines(Path.Combine(input, "compatibility.tsv")).Select(line => { if (line.StartsWith("DS2FX-")) return line; var fields = line.Split('\t'); Need(fields.Length == 5, "Malformed integration manifest"); fields[3] = Hash(File.ReadAllBytes(Path.Combine(output, fields[4]))); return string.Join('\t', fields); }));
var changed = pinned.Keys.Concat(new[] { "files.tsv", "compatibility.tsv" }).ToHashSet();
foreach (var pair in originalFiles) { Need(Hash(File.ReadAllBytes(Path.Combine(input, pair.Key))) == pair.Value, "Source changed during build"); if (!changed.Contains(pair.Key)) Need(Hash(File.ReadAllBytes(Path.Combine(output, pair.Key))) == pair.Value, "Unrelated pack file changed"); }
File.WriteAllText(Path.Combine(output, "local-coop.audit.json"), JsonSerializer.Serialize(new { Version = "1.0.1", Regulations = reports, SourceUnchanged = true, ChangedFiles = changed, ItemSchemaSHA256 = Hash(File.ReadAllBytes(Path.Combine(args[1], "ITEM_PARAM.xml"))), UsageSchemaSHA256 = Hash(File.ReadAllBytes(Path.Combine(args[1], "ITEM_USAGE_PARAM.xml"))), NativeChanges = false, RemoteReplicationChanges = false, LiveMultiplayerValidated = false }, new JsonSerializerOptions { WriteIndented = true }));
Console.WriteLine("PASS: five regulation candidates reread; Lantern-only owned reusable multiplayer/guest usage; existing rows and unrelated binder entries preserved; manifests regenerated; input pack untouched.");
