using System.Globalization;
using System.Security.Cryptography;
using System.Text.Json;
using System.Xml.Linq;
using SoulsFormats;

// Owned effect 6042 only. Action 75's second parameter is documented as the
// sound gain in the author's FFX reference; listening in Scholar remains QA.
internal static class LanternFlameAudio
{
    const string SourceHash = "44E5E05B8D21C26DAA82892C2739D566F10B541500D38C7DBBF8949ADAAF2588";
    const string EffectHash = "4A5EF814E2739786B4763A8205A8EE23236C5ECF1BA625F76DBAE24F23442F7A";
    static string Hash(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes));
    static XDocument Xml(byte[] bytes)
    {
        using var stream = new MemoryStream();
        FFXDLSE.Read(bytes).XmlSerialize(stream);
        stream.Position = 0;
        return XDocument.Load(stream);
    }
    static void Need(bool valid, string message) { if (!valid) throw new InvalidDataException(message); }

    internal static void Build(string input, string output, float gain)
    {
        output = Path.GetFullPath(output);
        Need(output.StartsWith(@"F:\Game Dev\01_Projects\Tools\DS2FX\local\", StringComparison.OrdinalIgnoreCase), "External DS2FX scratch output required.");
        Need(!File.Exists(output), "Output already exists; preserve prior candidates.");
        Need(float.IsFinite(gain) && gain > 0 && gain < 1, "Gain must be strictly between zero and one.");
        var source = File.ReadAllBytes(input);
        Need(Hash(source) == SourceHash, "Unknown Lantern SFX archive; explicit merge required.");
        var binder = BND4.Read(source);
        Need(binder.Files.Count == 1779, "Unexpected SFX entry count.");
        var before = binder.Files.Select(f => new { f.ID, f.Name, f.Flags, f.CompressionType, Hash = Hash(f.Bytes) }).ToArray();
        var effect = binder.Files.Single(f => f.ID == 6042 && f.Name.Replace('\\', '/').EndsWith("/f0006042.ffx", StringComparison.Ordinal));
        Need(Hash(effect.Bytes) == EffectHash, "Unknown owned Lantern flame.");
        var originalXml = Xml(effect.Bytes);
        var editedXml = new XDocument(originalXml);
        var action = editedXml.Descendants("Param").Single(p => (string?)p.Attribute("ActionID") == "75");
        var fields = action.Element("ParamList")!.Elements("Param").ToArray();
        XNamespace xsi = "http://www.w3.org/2001/XMLSchema-instance";
        Need(fields.Length == 3 && (string?)fields[0].Attribute(xsi + "type") == "Param68"
            && (string?)fields[0].Attribute("SoundID") == "157"
            && (string?)fields[1].Attribute(xsi + "type") == "Param7"
            && (string?)fields[1].Attribute("Float") == "1"
            && (string?)fields[2].Attribute(xsi + "type") == "Param1"
            && (string?)fields[2].Attribute("Int") == "1", "Unexpected Lantern sound layout.");
        fields[1].SetAttributeValue("Float", gain.ToString("R", CultureInfo.InvariantCulture));
        using var reader = editedXml.CreateReader();
        var candidate = FFXDLSE.XmlDeserialize(reader).Write();
        Need(candidate.Length == effect.Bytes.Length && XNode.DeepEquals(editedXml, Xml(candidate)), "FFX semantic/layout round-trip mismatch.");
        var restored = Xml(candidate);
        restored.Descendants("Param").Single(p => (string?)p.Attribute("ActionID") == "75")
            .Element("ParamList")!.Elements("Param").ElementAt(1).SetAttributeValue("Float", "1");
        Need(XNode.DeepEquals(originalXml, restored), "Non-audio flame data changed.");
        effect.Bytes = candidate;
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        binder.Write(output);
        var check = BND4.Read(output);
        Need(check.Files.Count == before.Length, "SFX entry count changed.");
        for (int index = 0; index < before.Length; index++)
        {
            var old = before[index]; var current = check.Files[index];
            Need(old.ID == current.ID && old.Name == current.Name && old.Flags == current.Flags
                && old.CompressionType == current.CompressionType, "SFX entry identity changed.");
            Need(Hash(current.Bytes) == (old.ID == 6042 ? Hash(candidate) : old.Hash), "Unexpected SFX entry modification.");
        }
        editedXml.Save(output + ".effect.xml");
        var audit = new
        {
            InputSHA256 = SourceHash, OutputSHA256 = Hash(File.ReadAllBytes(output)), EffectId = 6042,
            EffectInputSHA256 = EffectHash, EffectOutputSHA256 = Hash(candidate), SoundId = 157,
            GainBefore = 1f, GainAfter = gain, LoopUnchanged = true, VisualsUnchanged = true,
            OtherEntriesByteIdentical = 1778, Torch230ByteIdentical = true,
            SingleXmlAttributeChange = true, RoundTripPassed = true,
            Reference = "https://docs.google.com/spreadsheets/d/1OxvRUzu4eDoMzD6G26OtFnIyicYXyeJKE6BDi7hwj5Y/edit (sheet 75)",
            ScholarListeningVerified = false
        };
        File.WriteAllText(output + ".audit.json", JsonSerializer.Serialize(audit, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine(JsonSerializer.Serialize(audit));
    }
}
