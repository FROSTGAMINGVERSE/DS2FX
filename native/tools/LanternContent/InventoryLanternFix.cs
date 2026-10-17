using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Corrects the Torch-class research row and the pre-creation coat fixture.
static class InventoryLanternFix
{
    const int RepairPowder = 60410000, Lantern = 60425001;
    const int PrecreationRow = 10, DefaultPrecreationBody = 21100101, ResearchCoat = 21030101;
    const string ExpectedHash = "C87D1900669CAFFAE674D21712612AFC9EC1803C026670F3E2F66FBB513F384C";
    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static void GuardOutput(string path) => Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\", StringComparison.OrdinalIgnoreCase), "Output outside research workspace");

    public static void Build(string input, string schemaDirectory, string output)
    {
        GuardOutput(output); Require(!File.Exists(output), "Output already exists; use a new candidate filename");
        var baseline = File.ReadAllBytes(input); Require(Hash(baseline) == ExpectedHash, "Unexpected pickup candidate hash");
        var binder = BND4.Read(baseline); var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var defs = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        BinderFile Entry(string name) => binder.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == name);
        PARAM Decode(string name) { var p = PARAM.Read(Entry(name).Bytes); p.ApplyParamdef(defs[p.ParamType]); return p; }

        var items = Decode("ItemParam.param");
        var source = items.Rows.Single(row => row.ID == RepairPowder); var lantern = items.Rows.Single(row => row.ID == Lantern);
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 1300, "Lantern is no longer the expected Torch-class research row");
        for (var index = 0; index < source.Cells.Count; index++) lantern.Cells[index].Value = source.Cells[index].Value is byte[] bytes ? bytes.ToArray() : source.Cells[index].Value;
        lantern["iconId"].Value = Lantern; lantern["sortId"].Value = 302; lantern["maxNum"].Value = (byte)1;
        lantern["price"].Value = 0; lantern["salePrice"].Value = 0; lantern["specialEffectId"].Value = 0;
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 700 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 1700 && Convert.ToInt32(lantern["maxNum"].Value) == 1, "Lantern did not become an inventory utility Good");
        Entry("ItemParam.param").Bytes = items.Write();

        var starts = Decode("PlayerStatusParam.param"); var precreation = starts.Rows.Single(row => row.ID == PrecreationRow);
        Require(Convert.ToInt32(precreation["bodyArmor"].Value) == ResearchCoat, "Pre-creation outfit is not the expected research override");
        precreation["bodyArmor"].Value = DefaultPrecreationBody; Entry("PlayerStatusParam.param").Bytes = starts.Write();

        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(file => Path.GetFileName(file.Name.Replace('\\', '/'))).OrderBy(name => name).ToArray();
        var expected = new[] { "ItemParam.param", "PlayerStatusParam.param" };
        Require(changed.SequenceEqual(expected), "Unexpected regulation entries changed: " + string.Join(", ", changed));
        Directory.CreateDirectory(Path.GetDirectoryName(output)!); binder.Write(output);
        var reread = BND4.Read(output); Require(reread.Files.Count == binder.Files.Count, "Archive entry count changed");
        foreach (var file in binder.Files.Where(file => !changed.Contains(Path.GetFileName(file.Name.Replace('\\', '/'))))) Require(before[file.Name].SequenceEqual(reread.Files.Single(other => other.Name == file.Name).Bytes), "Unexpected unrelated entry difference: " + file.Name);
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(new {
            Status = "CONTENT FIX CANDIDATE. Lantern is a one-count inventory utility Good with no equipped-arm parameter. No quick-slot action or oil service is implemented.",
            InputSHA256 = Hash(baseline), OutputSHA256 = Hash(File.ReadAllBytes(output)), LanternItem = Lantern,
            ItemRoute = new { Source = RepairPowder, ItemType = 700, ItemUsage = 1700, Max = 1, SpecialEffect = 0, Icon = Lantern },
            RestoredPrecreationBody = new { Row = PrecreationRow, From = ResearchCoat, To = DefaultPrecreationBody }, ChangedEntries = changed, UntouchedEntries = binder.Files.Count - changed.Length, Deployed = false
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Inventory Lantern and default pre-creation outfit fix built and re-read.");
    }
}
