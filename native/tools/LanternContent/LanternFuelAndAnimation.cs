using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Restores Oil as the native, game-owned fuel timer and gives the reusable
// Lantern Lifegem's faster item-use timing. Illumination is gated in Core.
static class LanternFuelAndAnimation
{
    const int Lifegem = 60010000, Oil = 60425000, Lantern = 60425001, FirstTorchLot = 10026050;
    const string ExpectedHash = "6D44AF298E92DD3C05D61A8EEF0481A84ACB90D12ABDA450810C2036F1E50216";
    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static void GuardOutput(string path) => Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\", StringComparison.OrdinalIgnoreCase), "Output outside research workspace");

    public static void Build(string input, string schemaDirectory, string output)
    {
        GuardOutput(output); Require(!File.Exists(output), "Output already exists; use a new candidate filename");
        var baseline = File.ReadAllBytes(input); Require(Hash(baseline) == ExpectedHash, "Unexpected Oil-light-removal regulation hash");
        var binder = BND4.Read(baseline); var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var defs = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        BinderFile Entry(string name) => binder.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == name);
        PARAM Decode(string name) { var p = PARAM.Read(Entry(name).Bytes); p.ApplyParamdef(defs[p.ParamType]); return p; }

        var itemEntry = Entry("ItemParam.param"); var items = Decode("ItemParam.param");
        var lifegem = items.Rows.Single(row => row.ID == Lifegem); var lantern = items.Rows.Single(row => row.ID == Lantern); var oil = items.Rows.Single(row => row.ID == Oil);
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 260 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 2200 && Convert.ToInt32(lantern["specialEffectId"].Value) == Lantern, "Lantern is not the expected reusable toggle row");
        lantern["itemUsageParamId"].Value = lifegem["itemUsageParamId"].Value;
        Require(Convert.ToInt32(lantern["itemUsageParamId"].Value) == 1610, "Lifegem use timing was not applied");
        Require(Convert.ToInt32(oil["specialEffectId"].Value) == 0, "Oil is not the expected disabled fuel row");
        oil["specialEffectId"].Value = Oil; itemEntry.Bytes = items.Write();

        var eventEntry = Entry("SpEffectActiveItem.emevd"); var active = EMEVD.Read(eventEntry.Bytes);
        Require(active.Events.All(evt => evt.ID != Oil), "Oil fuel event already exists");
        var refill = new EMEVD.Event(Oil);
        // command 1000[9]: native status19, 300 seconds, PlayerLight51.
        // Core suppresses this owned light unless Lantern is visibly toggled on.
        refill.Instructions.Add(new EMEVD.Instruction(1000, 9, Convert.FromHexString("130000000000964333000000")));
        active.Events.Add(refill); active.Events = active.Events.OrderBy(evt => evt.ID).ToList(); eventEntry.Bytes = active.Write();

        var lots = Decode("ItemLotParam2_Other.param"); var lot = lots.Rows.Single(row => row.ID == FirstTorchLot);
        var oilSlots = Enumerable.Range(1, 10).Where(slot => Convert.ToInt32(lot[$"itemId{slot}"].Value) == Oil && Convert.ToInt32(lot[$"itemNum{slot}"].Value) > 0).ToArray();
        Require(oilSlots.Length == 1 && Convert.ToInt32(lot[$"itemNum{oilSlots[0]}"].Value) == 1, "Expected exactly one Lantern Oil in the first Torch corpse");
        lot[$"itemNum{oilSlots[0]}"].Value = (byte)3; Entry("ItemLotParam2_Other.param").Bytes = lots.Write();

        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(file => Path.GetFileName(file.Name.Replace('\\', '/'))).OrderBy(name => name).ToArray();
        var expected = new[] { "ItemLotParam2_Other.param", "ItemParam.param", "SpEffectActiveItem.emevd" }; Require(changed.SequenceEqual(expected), "Unexpected regulation entries changed: " + string.Join(", ", changed));
        Directory.CreateDirectory(Path.GetDirectoryName(output)!); binder.Write(output);
        var reread = BND4.Read(output); Require(reread.Files.Count == binder.Files.Count, "Archive entry count changed");
        var rereadEvents = EMEVD.Read(reread.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == "SpEffectActiveItem.emevd").Bytes);
        Require(rereadEvents.Events.Single(evt => evt.ID == Oil).Instructions.Single().ArgData.SequenceEqual(Convert.FromHexString("130000000000964333000000")), "Oil refill event round-trip failed");
        foreach (var file in binder.Files.Where(file => !changed.Contains(Path.GetFileName(file.Name.Replace('\\', '/'))))) Require(before[file.Name].SequenceEqual(reread.Files.Single(other => other.Name == file.Name).Bytes), "Unexpected unrelated entry difference: " + file.Name);
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(new {
            Status = "OFFLINE LAB CANDIDATE. Lantern uses Lifegem timing. Oil starts the native 300-second fuel timer; Core permits owned light only while Lantern toggle is visibly active.",
            InputSHA256 = Hash(baseline), OutputSHA256 = Hash(File.ReadAllBytes(output)), LanternItem = Lantern, OilItem = Oil,
            Animation = new {Source = Lifegem, ItemUsage = 1610}, Fuel = new {Seconds = 300, Light = 51, Event = Oil}, FirstTorchCorpse = new {Lot = FirstTorchLot, OilCount = 3}, ChangedEntries = changed, UntouchedEntries = binder.Files.Count - changed.Length, Deployed = false
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Lifegem-timed Lantern, native Oil fuel, and three-Oil first corpse candidate built and re-read.");
    }
}
