using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Places the existing Lantern Lab items on precisely one early-game corpse.
// It intentionally contains no input hook, animation call, light call, or save
// mutation; those need a separately proven native action route.
static class ThingsBetwixtPickup
{
    const int Torch = 60420000, Oil = 60425000, Lantern = 60425001;
    const int FirstTorchCorpseLot = 10026050;
    const string ExpectedHash = "192C396CA4D8075832E8EB2C69F2ED5AA716F0261CDE549A90F8120FE5D7C14A";

    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static void GuardOutput(string path) => Require(Path.GetFullPath(path).StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\", StringComparison.OrdinalIgnoreCase), "Output outside research workspace");

    public static void Build(string input, string schemaDirectory, string output)
    {
        GuardOutput(output);
        Require(!File.Exists(output), "Output already exists; use a new candidate filename");
        var baseline = File.ReadAllBytes(input);
        Require(Hash(baseline) == ExpectedHash, "Unexpected Lab regulation; refresh the exact input hash before altering loot");
        var binder = BND4.Read(baseline);
        var before = binder.Files.ToDictionary(file => file.Name, file => file.Bytes.ToArray());
        var defs = Directory.GetFiles(schemaDirectory, "*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(def => def.ParamType);
        BinderFile Entry(string name) => binder.Files.Single(file => Path.GetFileName(file.Name.Replace('\\', '/')) == name);
        PARAM Decode(string name) { var p = PARAM.Read(Entry(name).Bytes); p.ApplyParamdef(defs[p.ParamType]); return p; }

        var items = Decode("ItemParam.param");
        var lantern = items.Rows.SingleOrDefault(row => row.ID == Lantern);
        Require(lantern is not null && items.Rows.Any(row => row.ID == Oil), "Expected Lantern Lab item rows are absent");
        // The prior research candidate inherited Torch's icon.  Replace only
        // that presentation field with the owned inventory icon generated for
        // this item; no weapon/shield equipment parameter is introduced.
        lantern!["iconId"].Value = Lantern;
        Require(Convert.ToInt32(lantern["iconId"].Value) == Lantern, "Lantern row could not be mapped to its owned icon");
        Entry("ItemParam.param").Bytes = items.Write();

        var starts = Decode("PlayerStatusParam.param");
        var clearedSlots = new List<object>();
        foreach (var rowId in new[] { 10, 100 })
        {
            var row = starts.Rows.Single(row => row.ID == rowId);
            for (var slot = 1; slot <= 10; slot++)
            {
                var itemCell = row[$"beltItem{slot:D2}"];
                var numberCell = row[$"beltItem{slot:D2}Num"];
                var itemId = Convert.ToInt32(itemCell.Value);
                if (itemId != Oil && itemId != Lantern) continue;
                Require(Convert.ToInt32(numberCell.Value) > 0, $"Starting row {rowId} has invalid Lantern/Oil count in quick slot {slot}");
                itemCell.Value = -1;
                numberCell.Value = (ushort)0;
                clearedSlots.Add(new { Row = rowId, QuickSlot = slot, RemovedItem = itemId });
            }
        }
        Require(clearedSlots.Count > 0, "No starting Lantern/Oil quick-slot grant was found; refusing an unverifiable cleanup");
        Entry("PlayerStatusParam.param").Bytes = starts.Write();

        var lots = Decode("ItemLotParam2_Other.param");
        var lot = lots.Rows.Single(row => row.ID == FirstTorchCorpseLot);
        Require(Convert.ToInt32(lot["itemId1"].Value) == 60630000 && Convert.ToInt32(lot["itemNum1"].Value) > 0, "Target Things Betwixt lot no longer has its expected soul");
        Require(Convert.ToInt32(lot["itemId2"].Value) == Torch && Convert.ToInt32(lot["itemNum2"].Value) > 0, "Target Things Betwixt lot no longer has its expected Torch");
        var accessorySlots = Enumerable.Range(3, 8).Select(slot => new {
            Slot = slot,
            Id = Convert.ToInt32(lot[$"itemId{slot}"].Value),
            Amount = Convert.ToInt32(lot[$"itemNum{slot}"].Value),
            Weight = Convert.ToSingle(lot[$"itemWeight{slot}"].Value)
        }).ToArray();
        // The current Lab candidate already awards one Oil here.  It is an
        // intentional overlap, so retain it exactly and add only the missing
        // Lantern.  Any other occupied third-through-tenth slot is a conflict.
        Require(accessorySlots.Where(entry => entry.Amount > 0).All(entry => entry.Id == Oil), "Unexpected existing loot in the first-Torch corpse");
        Require(accessorySlots.Count(entry => entry.Amount > 0 && entry.Id == Oil) == 1, "Expected exactly one existing Lantern Oil pickup");
        Require(accessorySlots.All(entry => entry.Id != Lantern), "Lantern is already present in the first-Torch corpse");
        var lanternSlot = accessorySlots.FirstOrDefault(entry => entry.Amount == 0 && entry.Weight == 0f);
        Require(lanternSlot is not null, "First-Torch corpse has no empty loot slot for the Lantern");
        foreach (var (slot, itemId) in new[] { (lanternSlot!.Slot, Lantern) })
        {
            lot[$"itemId{slot}"].Value = itemId;
            lot[$"itemNum{slot}"].Value = (ushort)1;
            lot[$"itemWeight{slot}"].Value = (ushort)1;
            foreach (var suffix in new[] { "ReinforceLevel", "Attribute", "bExFlag" })
            {
                var destination = $"item{suffix}{slot}";
                var source = $"item{suffix}2";
                if (lot.Cells.Any(cell => cell.Def.InternalName == destination)) lot[destination].Value = lot[source].Value;
            }
        }
        Entry("ItemLotParam2_Other.param").Bytes = lots.Write();

        var changed = binder.Files.Where(file => !before[file.Name].SequenceEqual(file.Bytes)).Select(file => Path.GetFileName(file.Name.Replace('\\', '/'))).OrderBy(name => name).ToArray();
        var expected = new[] { "ItemLotParam2_Other.param", "ItemParam.param", "PlayerStatusParam.param" };
        Require(changed.SequenceEqual(expected), "Unexpected regulation entries changed: " + string.Join(", ", changed));
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        binder.Write(output);
        var reread = BND4.Read(output);
        Require(reread.Files.Count == binder.Files.Count, "Archive entry count changed");
        foreach (var file in binder.Files.Where(file => !changed.Contains(Path.GetFileName(file.Name.Replace('\\', '/')))))
            Require(before[file.Name].SequenceEqual(reread.Files.Single(other => other.Name == file.Name).Bytes), "Unexpected unrelated entry difference: " + file.Name);
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(new {
            Status = "CONTENT DELIVERY CANDIDATE. Grants Lantern and Oil only from Things Betwixt lot 10026050; no starting grant. It does not implement quick-slot activation or oil service.",
            InputSHA256 = Hash(baseline), OutputSHA256 = Hash(File.ReadAllBytes(output)), TargetLot = FirstTorchCorpseLot,
            ExistingLoot = new { Soul = 60630000, Torch }, AddedLoot = new { Lantern, Oil }, RemovedStarterQuickSlots = clearedSlots,
            ChangedEntries = changed, UntouchedEntries = binder.Files.Count - changed.Length, Deployed = false
        }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine("Things Betwixt first-Torch corpse candidate built and re-read; starter grants removed.");
    }

    public static void Text(string inputDirectory, string outputDirectory)
    {
        GuardOutput(outputDirectory);
        var texts = new Dictionary<string, Dictionary<int, string>> {
            ["simpleexplanation.fmg"] = new() {
                [Lantern] = "A reusable hip lantern. It never occupies either arm slot.",
                [Oil] = "Oil for the Wayfarer's Lantern."
            },
            ["detailedexplanation.fmg"] = new() {
                [Lantern] = "A brass lantern carried at the hip.\n\nIt is a utility item and never occupies either weapon or shield slot.",
                [Oil] = "A clear lamp oil blend carried by prepared wayfarers.\n\nUsed to service the Wayfarer's Lantern once its native service route is enabled."
            }
        };
        foreach (var (fileName, replacements) in texts)
        {
            var input = Path.Combine(inputDirectory, fileName); var output = Path.Combine(outputDirectory, fileName);
            Require(!File.Exists(output), "FMG output already exists");
            var fmg = FMG.Read(input); var before = fmg.Entries.Select(entry => (entry.ID, entry.Text)).ToArray();
            foreach (var (id, text) in replacements) { var entry = fmg.Entries.SingleOrDefault(item => item.ID == id); Require(entry is not null, "Expected Lantern Lab FMG entry absent: " + id); entry!.Text = text; }
            Directory.CreateDirectory(outputDirectory); fmg.Write(output); var reread = FMG.Read(output);
            foreach (var old in before.Where(old => !replacements.ContainsKey(old.ID))) Require(reread.Entries.Single(entry => entry.ID == old.ID).Text == old.Text, "Existing text changed");
            foreach (var replacement in replacements) Require(reread.Entries.Single(entry => entry.ID == replacement.Key).Text == replacement.Value, "Text replacement missing");
        }
        Console.WriteLine("Things Betwixt pickup text built and verified; no unimplemented toggle claim is shown.");
    }
}
