using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Repairs only the proved Lifegem-route regression: the Lantern remains its
// own item/event, but resumes the complete reusable Darksign item contract.
// It is a candidate builder; staging/deployment remains a separate step.
static class RestoreReusableLanternToggle
{
    const int Darksign = 60360000, Lantern = 60425001;
    const string ExpectedHash = "6F0C2D8940EA839A9056EE09F35545527F006F2344495EEDA76E71800F7B2B8D";
    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));

    public static void Build(string input, string schemaDirectory, string output)
    {
        Require(!File.Exists(output), "Output exists; use a fresh candidate path");
        var baseline = File.ReadAllBytes(input);
        Require(Hash(baseline) == ExpectedHash, "Unexpected Lab regulation; refusing reusable-toggle repair");
        var binder = BND4.Read(baseline);
        var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var definitions = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        var entry = binder.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == "ItemParam.param");
        var items = PARAM.Read(entry.Bytes); items.ApplyParamdef(definitions[items.ParamType]);
        var source = items.Rows.Single(row => row.ID == Darksign);
        var lantern = items.Rows.Single(row => row.ID == Lantern);

        Require(Convert.ToInt32(source["itemTypeParamId"].Value) == 260 && Convert.ToInt32(source["itemUsageParamId"].Value) == 2200 &&
                Convert.ToInt32(source["maxNum"].Value) == 1, "Darksign reusable source contract changed");
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 260 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 1610 &&
                Convert.ToInt32(lantern["specialEffectId"].Value) == Lantern && Convert.ToInt32(lantern["maxNum"].Value) == 1,
                "Lantern is not the known Lifegem-route regression");

        for (var index = 0; index < source.Cells.Count; index++)
            lantern.Cells[index].Value = source.Cells[index].Value is byte[] bytes ? bytes.ToArray() : source.Cells[index].Value;
        lantern["iconId"].Value = Lantern;
        lantern["sortId"].Value = 302;
        lantern["price"].Value = 0;
        lantern["salePrice"].Value = 0;
        lantern["specialEffectId"].Value = Lantern;

        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 260 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 2200 &&
                Convert.ToInt32(lantern["maxNum"].Value) == 1 && Convert.ToInt32(lantern["specialEffectId"].Value) == Lantern &&
                Convert.ToInt32(lantern["iconId"].Value) == Lantern, "Reusable Lantern contract verification failed");
        entry.Bytes = items.Write();
        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(file => Path.GetFileName(file.Name.Replace('\\', '/'))).OrderBy(name => name).ToArray();
        Require(changed.SequenceEqual(new[] { "ItemParam.param" }), "Unexpected archive changes: " + string.Join(", ", changed));
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        binder.Write(output);

        var reread = BND4.Read(output);
        var rereadEntry = reread.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == "ItemParam.param");
        var rereadItems = PARAM.Read(rereadEntry.Bytes); rereadItems.ApplyParamdef(definitions[rereadItems.ParamType]);
        var rereadLantern = rereadItems.Rows.Single(row => row.ID == Lantern);
        Require(Convert.ToInt32(rereadLantern["itemUsageParamId"].Value) == 2200 && Convert.ToInt32(rereadLantern["specialEffectId"].Value) == Lantern,
                "Reusable Lantern candidate round-trip failed");
        foreach (var original in binder.Files.Where(file => Path.GetFileName(file.Name.Replace('\\', '/')) != "ItemParam.param"))
            Require(before[original.Name].SequenceEqual(reread.Files.Single(file => file.Name == original.Name).Bytes), "Unexpected non-item archive difference");
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(new {
            Status = "CANDIDATE ONLY. Restores Darksign-style reusable item contract while preserving Lantern active-item event.",
            InputSHA256 = Hash(baseline), OutputSHA256 = Hash(File.ReadAllBytes(output)), LanternItem = Lantern,
            PreviousUsage = 1610, RestoredUsage = 2200, PreservedSpecialEffect = Lantern, ChangedEntries = changed, Deployed = false
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Reusable Lantern toggle candidate built and round-tripped; only ItemParam changed.");
    }
}
