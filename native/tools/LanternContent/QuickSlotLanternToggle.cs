using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Adds the owned, zero-duration active-item event which the native Core
// recognizes as the reusable Lantern's on/off request.
static class QuickSlotLanternToggle
{
    const int Lantern = 60425001;
    const string ExpectedHash = "EDFC8200E344DA545C4869A3F7501AA394A84AC5368B8371773398510B757985";
    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static void GuardOutput(string path) => Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\", StringComparison.OrdinalIgnoreCase), "Output outside research workspace");

    public static void Build(string input, string schemaDirectory, string output)
    {
        GuardOutput(output); Require(!File.Exists(output), "Output already exists; use a new candidate filename");
        var baseline = File.ReadAllBytes(input); Require(Hash(baseline) == ExpectedHash, "Unexpected reusable-Lantern regulation hash");
        var binder = BND4.Read(baseline); var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var defs = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        BinderFile Entry(string name) => binder.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == name);
        var itemEntry = Entry("ItemParam.param"); var items = PARAM.Read(itemEntry.Bytes); items.ApplyParamdef(defs[items.ParamType]);
        var lantern = items.Rows.Single(row => row.ID == Lantern);
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 260 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 2200 && Convert.ToInt32(lantern["specialEffectId"].Value) == 0, "Lantern is not the expected reusable pre-toggle row");
        lantern["specialEffectId"].Value = Lantern; itemEntry.Bytes = items.Write();

        var eventEntry = Entry("SpEffectActiveItem.emevd"); var active = EMEVD.Read(eventEntry.Bytes);
        Require(active.Events.All(evt => evt.ID != Lantern), "Lantern active-item event already exists");
        var eventData = new EMEVD.Event(Lantern);
        // command 1000[9]: ChrStatus_Value32(status19, duration0, value51).
        // Zero duration means no retained status/timer; Core recognizes this
        // exact owned command after native execution as a visual toggle signal.
        eventData.Instructions.Add(new EMEVD.Instruction(1000, 9, Convert.FromHexString("130000000000000033000000")));
        active.Events.Add(eventData); active.Events = active.Events.OrderBy(evt => evt.ID).ToList(); eventEntry.Bytes = active.Write();

        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(file => Path.GetFileName(file.Name.Replace('\\', '/'))).OrderBy(name => name).ToArray();
        var expected = new[] { "ItemParam.param", "SpEffectActiveItem.emevd" };
        Require(changed.SequenceEqual(expected), "Unexpected regulation entries changed: " + string.Join(", ", changed));
        Directory.CreateDirectory(Path.GetDirectoryName(output)!); binder.Write(output);
        var reread = BND4.Read(output); Require(reread.Files.Count == binder.Files.Count, "Archive entry count changed");
        var rereadEvent = EMEVD.Read(reread.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == "SpEffectActiveItem.emevd").Bytes).Events.Single(evt => evt.ID == Lantern);
        Require(rereadEvent.Instructions.Count == 1 && rereadEvent.Instructions[0].Bank == 1000 && rereadEvent.Instructions[0].ID == 9 && Convert.ToHexString(rereadEvent.Instructions[0].ArgData) == "130000000000000033000000", "Lantern toggle event round-trip failed");
        foreach (var file in binder.Files.Where(file => !changed.Contains(Path.GetFileName(file.Name.Replace('\\', '/'))))) Require(before[file.Name].SequenceEqual(reread.Files.Single(other => other.Name == file.Name).Bytes), "Unexpected unrelated entry difference: " + file.Name);
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(new {
            Status = "OFFLINE LAB CANDIDATE. A reusable Lantern active-item event emits a zero-duration status19/value51 signal for the exact native toggle observer. It has no item consumption or timer.",
            InputSHA256 = Hash(baseline), OutputSHA256 = Hash(File.ReadAllBytes(output)), LanternItem = Lantern,
            ItemRoute = new {Type = 260, Usage = 2200, SpecialEffect = Lantern, Max = 1}, Signal = new {Event = Lantern, Bank = 1000, Instruction = 9, Status = 19, Duration = 0, Value = 51}, ChangedEntries = changed, UntouchedEntries = binder.Files.Count - changed.Length, Deployed = false
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Quick-slot reusable Lantern toggle candidate built and re-read.");
    }
}
