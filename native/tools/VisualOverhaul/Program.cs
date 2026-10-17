using SoulsFormats;
using Org.BouncyCastle.Crypto;
using Org.BouncyCastle.Crypto.Engines;
using Org.BouncyCastle.OpenSsl;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;

if (args.Length != 4 || args[0] is not ("audit" or "resources" or "build" or "classify" or "billboards" or "types")) throw new ArgumentException("VisualOverhaul audit|resources|build|classify|billboards <Seeker zip> <Scholar Game directory> <fresh output>; types <effective regulation> <Scholar Game directory> <fresh output>");
var archive = Path.GetFullPath(args[1]); var game = Path.GetFullPath(args[2]); var output = Path.GetFullPath(args[3]);
if(args[0] is "build" or "classify" &&DigestStream(archive)!="f6b191142dd4c2b0adb97589a392fdb9a3db0320e92eb1f4e6d8631570aa04b6") throw new InvalidDataException("Requires the audited Seeker of Fire Seamless 2.1.5 archive; run a new audit before accepting another release");
if (Directory.Exists(output) || output.StartsWith(game + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new IOException("Require fresh output outside game");
if (Digest(File.ReadAllBytes(Path.Combine(game, "DarkSoulsII.exe"))) != "0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131") throw new InvalidDataException("Unsupported Scholar executable");
if (DigestStream(Path.Combine(game,"GameDataEbl.bhd")) != "58465c59d5b16797964485a7db8d700db712e2287361e1cdb09f45407145bbca") throw new InvalidDataException("Unsupported vanilla Scholar archive index");
Directory.CreateDirectory(output);
if (args[0] == "types") { SafeScenery.VerifyEffectiveTypes(archive,game,output); return; }
using var vanilla = new GameArchive(game);
using var zip = ZipFile.OpenRead(archive);
if (args[0] == "billboards") { BillboardAudit.Run(zip,vanilla,output); return; }
if (args[0] == "classify") { SceneryAudit.Run(zip,vanilla,output,game); return; }
if (args[0] == "build") { VisualBuild.Run(zip, vanilla, output,game); return; }
if (args[0] == "resources") {
    var report = new List<object>();
    new BXF4().Write(out _, out byte[] emptyData);
    foreach (var entry in zip.Entries.Where(e => !e.FullName.EndsWith('/') && (e.FullName.Contains("/model/obj/") || e.FullName.EndsWith(".mapbhd")))) {
        string relative=entry.FullName["SeekerOfFire_Seamless/".Length..];
        using var buf=new MemoryStream(); using(var s=entry.Open())s.CopyTo(buf);var bytes=buf.ToArray();
        byte[]? baseline=null;try { baseline=vanilla.Read(relative); } catch(FileNotFoundException) { }
        if(relative.EndsWith(".mapbhd")) {
            using var reader=new BXF4Reader(bytes,emptyData);
            report.Add(new{relative, vanilla_exists=baseline!=null, identical=baseline!=null&&Digest(bytes)==Digest(baseline),
                files=reader.Files.Select(f=>new{f.Name,f.ID,f.Flags}).ToArray()});
        } else if (!BND4.Is(bytes)) {
            report.Add(new { relative, vanilla_exists=baseline!=null, bytes=bytes.Length, parse="not-BND4-or-empty", magic=Convert.ToHexString(bytes.Take(16).ToArray()) });
        } else {
            var b=BND4.Read(bytes);
            report.Add(new{relative, vanilla_exists=baseline!=null, identical=baseline!=null&&Digest(bytes)==Digest(baseline),
                files=b.Files.Select(f=>new{f.Name,f.ID,f.Flags,bytes=f.Bytes.Length,sha256=Digest(f.Bytes)}).ToArray()});
        }
    }
    foreach(var entry in zip.Entries.Where(e=>e.FullName.EndsWith(".msb"))) {
        var id=Path.GetFileNameWithoutExtension(entry.FullName);var relative=$"model/map/{id}.mapbhd";
        using var reader=new BXF4Reader(vanilla.Read(relative),emptyData);
        report.Add(new{relative, source="vanilla", files=reader.Files.Select(f=>new{f.Name,f.ID,f.Flags}).ToArray()});
    }
    Save(Path.Combine(output,"resources.json"),report); Console.WriteLine($"Inspected {report.Count} model containers"); return;
}
var inventory = new List<object>(); var maps = new List<object>();
foreach (var entry in zip.Entries.Where(e => !e.FullName.EndsWith('/')).OrderBy(e => e.FullName, StringComparer.Ordinal)) {
    var path = entry.FullName.Replace('\\', '/');
    if (path.StartsWith('/') || path.Contains(':') || path.Split('/').Any(p => p is "." or "..")) throw new InvalidDataException("Unsafe archive entry");
    using var stream = entry.Open(); var hash = Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
    var relative = path.StartsWith("SeekerOfFire_Seamless/") ? path["SeekerOfFire_Seamless/".Length..] : path;
    inventory.Add(new { path, relative, size = entry.Length, sha256 = hash, category = Category(relative) });
    if (!relative.EndsWith(".msb", StringComparison.OrdinalIgnoreCase)) continue;
    using var bytes = new MemoryStream(); using (var s = entry.Open()) s.CopyTo(bytes);
    var source = bytes.ToArray(); var baseBytes = vanilla.Read(relative); var b = MSB2.Read(baseBytes); var m = MSB2.Read(source);
    if (b.Format != MSB2.MSBFormat.DarkSouls2Scholar || m.Format != b.Format) throw new InvalidDataException("Wrong MSB format");
    var folder = Path.Combine(output, "maps", Path.GetFileNameWithoutExtension(relative)); Directory.CreateDirectory(folder);
    File.WriteAllBytes(Path.Combine(folder, "vanilla.msb"), baseBytes); File.WriteAllBytes(Path.Combine(folder, "seeker.msb"), source);
    var diffs = new List<object>();
    foreach (var type in new[] { "MapPiece", "Object", "Collision", "Navmesh", "ConnectCollision" }) {
        var old = b.Parts.GetEntries().Where(p => p.GetType().Name == type).ToDictionary(p => p.Name);
        var mod = m.Parts.GetEntries().Where(p => p.GetType().Name == type).ToDictionary(p => p.Name);
        var added = mod.Keys.Except(old.Keys).Order().Select(n => Snap(mod[n])).ToArray();
        var removed = old.Keys.Except(mod.Keys).Order().Select(n => Snap(old[n])).ToArray();
        var changed = old.Keys.Intersect(mod.Keys).Order().Where(n => Canon(old[n]) != Canon(mod[n]))
            .Select(n => new { name = n, before = Snap(old[n]), after = Snap(mod[n]) }).ToArray();
        diffs.Add(new { type, vanilla = old.Count, seeker = mod.Count, added, removed, changed });
        Console.WriteLine($"{Path.GetFileNameWithoutExtension(relative)} {type}: +{added.Length} -{removed.Length} ~{changed.Length}");
    }
    var report = new { relative, vanilla_sha256 = Digest(baseBytes), seeker_sha256 = hash,
        parts = diffs, vanilla_models = b.Models.GetEntries().Select(Snap).ToArray(), seeker_models = m.Models.GetEntries().Select(Snap).ToArray(),
        vanilla_events = b.Events.GetEntries().Select(Snap).ToArray(), seeker_events = m.Events.GetEntries().Select(Snap).ToArray(),
        vanilla_regions = b.Regions.GetEntries().Select(Snap).ToArray(), seeker_regions = m.Regions.GetEntries().Select(Snap).ToArray(),
        vanilla_poses = b.PartPoses.Select(Snap).ToArray(), seeker_poses = m.PartPoses.Select(Snap).ToArray() };
    Save(Path.Combine(folder, "diff.json"), report); maps.Add(new { relative, folder, parts = diffs });
    // Verify the actual parser/writer before using it for a candidate.
    var roundtrip = MSB2.Read(b.Write());
    if (MapCanon(b) != MapCanon(roundtrip)) throw new InvalidDataException("Vanilla MSB semantic roundtrip changed " + relative);
}
Save(Path.Combine(output, "inventory.json"), inventory);
Save(Path.Combine(output, "maps.json"), maps);
Save(Path.Combine(output, "source.json"), new { archive, sha256 = DigestStream(archive), files = inventory.Count, maps = maps.Count,
    game, bhd_sha256 = DigestStream(Path.Combine(game,"GameDataEbl.bhd")), soulsformats_sha256 = DigestStream(typeof(MSB2).Assembly.Location),
    scope = "Local analysis only. Seeker-derived assets require author permission before distribution." });
Console.WriteLine($"Audited {inventory.Count} entries and {maps.Count} maps.");

static string Category(string p) => p.StartsWith("map/") ? p.EndsWith(".msb") ? "mixed-map-layout" : "navigation-or-map-index" :
    p.StartsWith("model/map/") ? "map-render-or-collision-archive" : p.StartsWith("model/obj/") ? "object-model" :
    p.StartsWith("model_lq/") ? "low-quality-model" : p.StartsWith("model/") ? "character-equipment-or-other-model" :
    p.StartsWith("filter/") ? "lighting-filter" : p.StartsWith("Param/") || p == "enc_regulation.bnd.dcx" ? "gameplay-parameters" :
    p.StartsWith("ezstate/") ? "event-script" : p.StartsWith("menu/") ? "ui-texture-or-text" : p.StartsWith("sound/") ? "sound" :
    p.StartsWith("sfx/") ? "effects" : "loader-or-seamless-settings";
static string Digest(byte[] b) => Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
static string DigestStream(string p) { using var s = File.OpenRead(p); return Convert.ToHexString(SHA256.HashData(s)).ToLowerInvariant(); }
static object Snap(object o) => new { type = o.GetType().Name, value = JsonSerializer.SerializeToElement(o, o.GetType(), new JsonSerializerOptions { IncludeFields = true }) };
static string Canon(object o) => JsonSerializer.Serialize(Snap(o));
static string MapCanon(MSB2 m) => string.Join('\n', m.Models.GetEntries().Select(Canon).Concat(m.Events.GetEntries().Select(Canon)).Concat(m.Regions.GetEntries().Select(Canon)).Concat(m.Parts.GetEntries().Select(Canon)).Concat(m.PartPoses.Select(Canon)));
static void Save(string p, object o) => File.WriteAllText(p, JsonSerializer.Serialize(o, new JsonSerializerOptions { WriteIndented = true }));

sealed class GameArchive : IDisposable {
    readonly ILookup<ulong, BHD5.FileHeader> files; readonly FileStream data;
    public GameArchive(string game) {
        var rsa = new RsaEngine(); using (var pem = File.OpenText(Path.Combine(game,"GameDataKeyCode.pem"))) rsa.Init(false,(AsymmetricKeyParameter)new PemReader(pem).ReadObject());
        using var clear = new MemoryStream(); using (var encrypted = File.OpenRead(Path.Combine(game,"GameDataEbl.bhd"))) {
            byte[] block = new byte[rsa.GetInputBlockSize()]; int n;
            while ((n = encrypted.Read(block)) != 0) { if (n != block.Length) throw new InvalidDataException("Partial RSA block");
                var decoded = rsa.ProcessBlock(block,0,n); var padded = new byte[rsa.GetOutputBlockSize()]; decoded.CopyTo(padded,padded.Length-decoded.Length); clear.Write(padded); }
        }
        clear.Position=0; files=BHD5.Read(clear,BHD5.Game.DarkSouls2).Buckets.SelectMany(b=>b).ToLookup(f=>f.FileNameHash);
        data=File.OpenRead(Path.Combine(game,"GameDataEbl.bdt"));
    }
    public byte[] Read(string path) {
        var normalized="/"+path.Replace('\\','/').TrimStart('/').ToLowerInvariant();
        uint hash=normalized.Aggregate(0u,(h,c)=>unchecked(h*37+c)); var hits=files[hash].ToArray();
        if(hits.Length==0) throw new FileNotFoundException("Missing vanilla asset: "+normalized,normalized);
        if(hits.Length!=1) throw new InvalidDataException("Ambiguous vanilla asset: "+normalized);
        return hits[0].ReadFile(data);
    }
    public void Dispose()=>data.Dispose();
}
