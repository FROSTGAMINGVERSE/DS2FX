using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Retires the old Oil-driven 300s PlayerLight51 timer. The reusable Lantern
// toggle is now the only allowed source of lantern illumination.
static class RemoveLegacyOilLight
{
    const int Oil = 60425000;
    const string ExpectedHash = "FAD12D474F13A9CD160416416635852B1E9669BD123851155A39F36C8C2DDEAD";
    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static void GuardOutput(string path) => Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\", StringComparison.OrdinalIgnoreCase), "Output outside research workspace");

    public static void Build(string input, string schemaDirectory, string output)
    {
        GuardOutput(output); Require(!File.Exists(output), "Output already exists; use a new candidate filename");
        var baseline = File.ReadAllBytes(input); Require(Hash(baseline) == ExpectedHash, "Unexpected quick-slot Lantern candidate hash");
        var binder = BND4.Read(baseline); var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var defs = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        BinderFile Entry(string name) => binder.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == name);
        var itemEntry = Entry("ItemParam.param"); var items = PARAM.Read(itemEntry.Bytes); items.ApplyParamdef(defs[items.ParamType]);
        var oil = items.Rows.Single(row => row.ID == Oil);
        Require(Convert.ToInt32(oil["specialEffectId"].Value) == Oil, "Oil does not use the expected legacy active-item event");
        oil["specialEffectId"].Value = 0; itemEntry.Bytes = items.Write();
        var eventEntry = Entry("SpEffectActiveItem.emevd"); var active = EMEVD.Read(eventEntry.Bytes);
        var legacy = active.Events.SingleOrDefault(evt => evt.ID == Oil); Require(legacy is not null, "Legacy Oil event missing");
        Require(legacy!.Instructions.Count == 2 && legacy.Instructions.Any(i => i.Bank == 1000 && i.ID == 9 && Convert.ToHexString(i.ArgData) == "130000000000964333000000"), "Legacy Oil event differs; refusing removal");
        active.Events.Remove(legacy); eventEntry.Bytes = active.Write();
        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(file => Path.GetFileName(file.Name.Replace('\\', '/'))).OrderBy(name => name).ToArray();
        var expected = new[] { "ItemParam.param", "SpEffectActiveItem.emevd" }; Require(changed.SequenceEqual(expected), "Unexpected regulation entries changed");
        Directory.CreateDirectory(Path.GetDirectoryName(output)!); binder.Write(output);
        var reread = BND4.Read(output); Require(reread.Files.Count == binder.Files.Count, "Archive entry count changed");
        var rereadEvents = EMEVD.Read(reread.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == "SpEffectActiveItem.emevd").Bytes);
        Require(rereadEvents.Events.All(evt => evt.ID != Oil) && rereadEvents.Events.Any(evt => evt.ID == 60425001), "Oil removal or Lantern toggle event verification failed");
        foreach (var file in binder.Files.Where(file => !changed.Contains(Path.GetFileName(file.Name.Replace('\\', '/'))))) Require(before[file.Name].SequenceEqual(reread.Files.Single(other => other.Name == file.Name).Bytes), "Unexpected unrelated entry difference: " + file.Name);
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(new {
            Status = "CONTENT FIX CANDIDATE. Removes the legacy Oil timer/PlayerLight51 activation. Oil has no active effect until the Lantern-visible refill route is implemented.",
            InputSHA256 = Hash(baseline), OutputSHA256 = Hash(File.ReadAllBytes(output)), OilItem = Oil,
            RemovedEvent = Oil, DisabledOilSpecialEffect = true, PreservedLanternToggleEvent = 60425001, ChangedEntries = changed, UntouchedEntries = binder.Files.Count - changed.Length, Deployed = false
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Legacy Oil light/timer removed; Lantern toggle remains intact.");
    }
}
