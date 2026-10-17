using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Makes the Lantern a reusable system-item-style utility.  Its Darksign action
// is explicitly removed; this is only the non-consumption route, not a toggle.
static class ReusableLanternFix
{
    const int Darksign = 60360000, Lantern = 60425001;
    const string ExpectedHash = "60CD2E725A488A08EC64E53D201189C27248EAE9A8348359757F8CB01DD66E29";
    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static void GuardOutput(string path) => Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\", StringComparison.OrdinalIgnoreCase), "Output outside research workspace");

    public static void Build(string input, string schemaDirectory, string output)
    {
        GuardOutput(output); Require(!File.Exists(output), "Output already exists; use a new candidate filename");
        var baseline = File.ReadAllBytes(input); Require(Hash(baseline) == ExpectedHash, "Unexpected Lantern regulation hash");
        var binder = BND4.Read(baseline); var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var defs = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        var entry = binder.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == "ItemParam.param");
        var items = PARAM.Read(entry.Bytes); items.ApplyParamdef(defs[items.ParamType]);
        var source = items.Rows.Single(row => row.ID == Darksign); var lantern = items.Rows.Single(row => row.ID == Lantern);
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 700, "Lantern is not the expected consumable candidate");
        for (var index = 0; index < source.Cells.Count; index++) lantern.Cells[index].Value = source.Cells[index].Value is byte[] bytes ? bytes.ToArray() : source.Cells[index].Value;
        lantern["iconId"].Value = Lantern; lantern["sortId"].Value = 302; lantern["price"].Value = 0; lantern["salePrice"].Value = 0;
        lantern["specialEffectId"].Value = 0;
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 260 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 2200 && Convert.ToInt32(lantern["maxNum"].Value) == 1 && Convert.ToInt32(lantern["specialEffectId"].Value) == 0, "Lantern did not become a safe reusable utility item");
        entry.Bytes = items.Write();
        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(file => Path.GetFileName(file.Name.Replace('\\', '/'))).OrderBy(name => name).ToArray();
        Require(changed.SequenceEqual(new[] { "ItemParam.param" }), "Unexpected regulation entries changed");
        Directory.CreateDirectory(Path.GetDirectoryName(output)!); binder.Write(output);
        var reread = BND4.Read(output); Require(reread.Files.Count == binder.Files.Count, "Archive entry count changed");
        foreach (var file in binder.Files.Where(file => file.Name != entry.Name)) Require(before[file.Name].SequenceEqual(reread.Files.Single(other => other.Name == file.Name).Bytes), "Unexpected unrelated entry difference: " + file.Name);
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(new {
            Status = "CONTENT FIX CANDIDATE. Lantern uses the reusable Darksign-style item route with its original special effect removed. No lantern toggle is implemented.",
            InputSHA256 = Hash(baseline), OutputSHA256 = Hash(File.ReadAllBytes(output)), LanternItem = Lantern,
            ItemRoute = new { Source = Darksign, ItemType = 260, ItemUsage = 2200, Max = 1, SpecialEffect = 0, Icon = Lantern }, ChangedEntries = changed, UntouchedEntries = binder.Files.Count - changed.Length, Deployed = false
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Reusable Darksign-style Lantern candidate built and re-read.");
    }
}
