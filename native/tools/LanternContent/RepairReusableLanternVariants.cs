using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Builds a complete, non-deployed regulation set for every Lantern Lab
// selection.  It deliberately restores the Darksign item-use contract rather
// than borrowing any consumable item route.
static class RepairReusableLanternVariants
{
    const int Darksign = 60360000, Lantern = 60425001;
    const string Signal = "130000000000000033000000";
    static readonly Dictionary<string, bool> Inputs = new(StringComparer.OrdinalIgnoreCase) {
        ["6F0C2D8940EA839A9056EE09F35545527F006F2344495EEDA76E71800F7B2B8D"] = true,
        ["D3883AF549618C95B69244036119D3A5CAEDD1E6F01CD0392E9D5334B24174D1"] = false,
        ["D126E4C55B8EB48AD281550833D10148308B7F462B190383A48E1C49001FEF1F"] = false,
        ["7DD8908DD0E9DEA928657DB2BD0151730DCAE93F07165C995E44AD5793DECBCA"] = false,
    };

    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static string Name(BinderFile file) => Path.GetFileName(file.Name.Replace('\\', '/'));
    static BinderFile Entry(BND4 binder, string name) => binder.Files.Single(file => Name(file) == name);

    static void CopyCells(PARAM.Row destination, PARAM.Row source)
    {
        for (var index = 0; index < source.Cells.Count; index++)
            destination.Cells[index].Value = source.Cells[index].Value is byte[] bytes ? bytes.ToArray() : source.Cells[index].Value;
    }

    static void VerifyReusable(PARAM.Row lantern)
    {
        Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 260 &&
                Convert.ToInt32(lantern["itemUsageParamId"].Value) == 2200 &&
                Convert.ToInt32(lantern["maxNum"].Value) == 1 &&
                Convert.ToInt32(lantern["specialEffectId"].Value) == Lantern &&
                Convert.ToInt32(lantern["iconId"].Value) == Lantern,
                "Reusable Lantern item contract verification failed");
    }

    static void VerifySignal(EMEVD active)
    {
        var evt = active.Events.Single(e => e.ID == Lantern);
        Require(evt.Instructions.Count == 1 && evt.Instructions[0].Bank == 1000 && evt.Instructions[0].ID == 9 &&
                Convert.ToHexString(evt.Instructions[0].ArgData) == Signal, "Lantern active-item signal is not the supported exact event");
    }

    static object BuildOne(string input, string schemaDirectory, string output)
    {
        Require(!File.Exists(output), "Output already exists: " + output);
        var baseline = File.ReadAllBytes(input);
        var inputHash = Hash(baseline);
        Require(Inputs.TryGetValue(inputHash, out var hasSignal), "Unexpected Lantern Lab regulation hash: " + inputHash);
        var binder = BND4.Read(baseline);
        var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var definitions = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        var itemEntry = Entry(binder, "ItemParam.param");
        var items = PARAM.Read(itemEntry.Bytes); items.ApplyParamdef(definitions[items.ParamType]);
        var source = items.Rows.Single(row => row.ID == Darksign);
        var lantern = items.Rows.Single(row => row.ID == Lantern);
        Require(Convert.ToInt32(source["itemTypeParamId"].Value) == 260 && Convert.ToInt32(source["itemUsageParamId"].Value) == 2200 && Convert.ToInt32(source["maxNum"].Value) == 1,
                "Darksign reusable source contract changed");
        if (hasSignal)
            Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 260 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 1610 && Convert.ToInt32(lantern["specialEffectId"].Value) == Lantern,
                    "Base regulation is not the known Lifegem-route regression");
        else
            Require(Convert.ToInt32(lantern["itemTypeParamId"].Value) == 1300 && Convert.ToInt32(lantern["itemUsageParamId"].Value) == 1700 && Convert.ToInt32(lantern["specialEffectId"].Value) == 0,
                    "Variant regulation is not the known legacy Lantern row");

        CopyCells(lantern, source);
        lantern["iconId"].Value = Lantern;
        lantern["sortId"].Value = 302;
        lantern["price"].Value = 0;
        lantern["salePrice"].Value = 0;
        lantern["specialEffectId"].Value = Lantern;
        VerifyReusable(lantern);
        itemEntry.Bytes = items.Write();

        if (hasSignal)
            VerifySignal(EMEVD.Read(Entry(binder, "SpEffectActiveItem.emevd").Bytes));
        else
        {
            var eventEntry = Entry(binder, "SpEffectActiveItem.emevd");
            var active = EMEVD.Read(eventEntry.Bytes);
            Require(active.Events.All(evt => evt.ID != Lantern), "Legacy variant unexpectedly already owns Lantern signal");
            var evt = new EMEVD.Event(Lantern);
            evt.Instructions.Add(new EMEVD.Instruction(1000, 9, Convert.FromHexString(Signal)));
            active.Events.Add(evt); active.Events = active.Events.OrderBy(evt => evt.ID).ToList();
            eventEntry.Bytes = active.Write();
        }

        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(Name).OrderBy(name => name).ToArray();
        var expected = hasSignal ? new[] { "ItemParam.param" } : new[] { "ItemParam.param", "SpEffectActiveItem.emevd" };
        Require(changed.SequenceEqual(expected), "Unexpected changed entries: " + string.Join(", ", changed));
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        binder.Write(output);

        var reread = BND4.Read(output);
        Require(reread.Files.Count == binder.Files.Count, "Archive entry count changed");
        var rereadItems = PARAM.Read(Entry(reread, "ItemParam.param").Bytes); rereadItems.ApplyParamdef(definitions[rereadItems.ParamType]);
        VerifyReusable(rereadItems.Rows.Single(row => row.ID == Lantern));
        VerifySignal(EMEVD.Read(Entry(reread, "SpEffectActiveItem.emevd").Bytes));
        foreach (var file in binder.Files.Where(file => !changed.Contains(Name(file))))
            Require(before[file.Name].SequenceEqual(reread.Files.Single(other => other.Name == file.Name).Bytes), "Unexpected unrelated difference: " + file.Name);
        return new { Input = input, InputSHA256 = inputHash, Output = output, OutputSHA256 = Hash(File.ReadAllBytes(output)), AddedSignal = !hasSignal, ChangedEntries = changed };
    }

    public static void Build(string baseInput, string standardInput, string estusOnInput, string estusOffInput, string schemaDirectory, string outputDirectory)
    {
        Require(!Directory.Exists(outputDirectory), "Output directory already exists; use a fresh candidate path");
        Directory.CreateDirectory(outputDirectory);
        try
        {
            var outputs = new[] {
                BuildOne(baseInput, schemaDirectory, Path.Combine(outputDirectory, "enc_regulation.bnd.dcx")),
                BuildOne(standardInput, schemaDirectory, Path.Combine(outputDirectory, "standard-jump.bnd.dcx")),
                BuildOne(estusOnInput, schemaDirectory, Path.Combine(outputDirectory, "estus-off-jump-on.bnd.dcx")),
                BuildOne(estusOffInput, schemaDirectory, Path.Combine(outputDirectory, "estus-off-jump-off.bnd.dcx")),
            };
            File.WriteAllText(Path.Combine(outputDirectory, "reusable-lantern-variants.audit.json"), JsonSerializer.Serialize(new {
                Status = "CANDIDATE ONLY. Restores reusable Darksign item route in all Lantern Lab regulations; legacy variants receive the exact owned Lantern active-item signal.",
                LanternItem = Lantern, ReusableUsage = 2200, Capacity = 1, Outputs = outputs, Deployed = false
            }, new JsonSerializerOptions { WriteIndented = true }));
            Console.WriteLine("Reusable Lantern candidates built and round-tripped for all four Lantern Lab regulations.");
        }
        catch { Directory.Delete(outputDirectory, true); throw; }
    }
}
