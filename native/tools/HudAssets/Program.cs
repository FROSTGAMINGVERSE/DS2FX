using SoulsFormats;
using Org.BouncyCastle.Crypto;
using Org.BouncyCastle.Crypto.Engines;
using Org.BouncyCastle.OpenSsl;
using System.Security.Cryptography;
using System.Text.Json;

if(args.Length!=3)throw new ArgumentException("HudAssets <Scholar Game directory> <dictionary> <new output directory>");
string game=Path.GetFullPath(args[0]),output=Path.GetFullPath(args[2]);
if(Directory.Exists(output) || output.StartsWith(game+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))
    throw new IOException("Require a fresh output directory outside the game");
string Hash(string p)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(p))).ToLowerInvariant();
if(Hash(Path.Combine(game,"DarkSoulsII.exe"))!="0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131")
    throw new InvalidDataException("Unsupported Scholar executable");
var paths=File.ReadAllLines(args[1]).Where(x=>x.StartsWith("/menu/tex/icon/vow/vi_",StringComparison.Ordinal) ||
    x.StartsWith("/menu/tex/icon/effect/ei_",StringComparison.Ordinal)).Distinct().Order().ToArray();
if(paths.Length<10 || paths.Length>512)throw new InvalidDataException("Unexpected icon dictionary");
var engine=new RsaEngine();
using(var sr=File.OpenText(Path.Combine(game,"GameDataKeyCode.pem")))
    engine.Init(false,(AsymmetricKeyParameter)new PemReader(sr).ReadObject());
using var clear=new MemoryStream();
using(var encrypted=File.OpenRead(Path.Combine(game,"GameDataEbl.bhd"))) {
    byte[] block=new byte[engine.GetInputBlockSize()];int size;
    while((size=encrypted.Read(block))!=0) {
        if(size!=block.Length)throw new InvalidDataException("Partial BHD RSA block");
        byte[] decoded=engine.ProcessBlock(block,0,size),padded=new byte[engine.GetOutputBlockSize()];
        decoded.CopyTo(padded,padded.Length-decoded.Length);clear.Write(padded);
    }
}
clear.Position=0;var bhd=BHD5.Read(clear,BHD5.Game.DarkSouls2);
uint PathHash(string path)=>path.ToLowerInvariant().Aggregate(0u,(h,c)=>unchecked(h*37+c));
var files=bhd.Buckets.SelectMany(x=>x).ToLookup(x=>x.FileNameHash);
using var data=File.OpenRead(Path.Combine(game,"GameDataEbl.bdt"));
Directory.CreateDirectory(output);var manifest=new List<object>();
foreach(var path in paths) {
    var entries=files[PathHash(path)].ToArray();
    if(entries.Length!=1)throw new InvalidDataException("Missing/ambiguous icon: "+path);
    byte[] tpfBytes=entries[0].ReadFile(data);var tpf=TPF.Read(tpfBytes);
    string name=Path.GetFileNameWithoutExtension(path);
    if(tpf.Platform!=TPF.TPFPlatform.PC || tpf.Textures.Count!=1 ||
       !string.Equals(tpf.Textures[0].Name,name,StringComparison.OrdinalIgnoreCase))
        throw new InvalidDataException("Icon texture contract: "+path);
    byte[] dds=tpf.Textures[0].Bytes;
    if(dds.Length<128 || !dds.AsSpan(0,4).SequenceEqual("DDS "u8))throw new InvalidDataException("Icon is not DDS");
    File.WriteAllBytes(Path.Combine(output,name+".dds"),dds);
    manifest.Add(new{archivePath=path,name,tpf_sha256=Convert.ToHexString(SHA256.HashData(tpfBytes)).ToLowerInvariant(),dds_sha256=Hash(Path.Combine(output,name+".dds"))});
}
File.WriteAllText(Path.Combine(output,"source-manifest.json"),JsonSerializer.Serialize(new{scope="Local game-derived artwork. Do not redistribute.",
    game_sha256=Hash(Path.Combine(game,"DarkSoulsII.exe")),bhd_sha256=Hash(Path.Combine(game,"GameDataEbl.bhd")),icons=manifest},new JsonSerializerOptions{WriteIndented=true}));
Console.WriteLine($"Extracted {manifest.Count} original icon DDS files to {output}");
