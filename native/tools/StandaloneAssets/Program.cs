using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.IO.Compression;
using System.Buffers.Binary;
using System.Text.Json;

static class Program {
record Asset(string Key,string SourceHash,string TargetHash,int TargetDcx,string Patch,long Size);
static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
const string GameHash="0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131";
static void Need(bool test,string reason){if(!test)throw new InvalidDataException(reason);}
static string Safe(string root,string relative){
    Need(!Path.IsPathRooted(relative) && !relative.Contains(':') && !relative.Split('/','\\').Any(x=>x==".." || x=="."),"Unsafe asset path");
    var result=Path.GetFullPath(Path.Combine(root,relative));Need(result.StartsWith(Path.GetFullPath(root)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Path escaped output");return result;
}
static byte[] Expand(byte[] bytes,out int type){type=-1;if(!DCX.Is(bytes))return bytes;var clear=DCX.Decompress(bytes,out var value);type=(int)value;return clear;}
static byte[] ExpandSource(string key,byte[] bytes){
    if(key=="enc_regulation.bnd.dcx" && !DCX.Is(bytes) && !BND4.Is(bytes)){
        // Scholar's public AES-CTR regulation format; see THIRD-PARTY.md.
        byte[] counter=new byte[16];counter[0]=128;bytes.AsSpan(0,11).CopyTo(counter.AsSpan(1));counter[15]=1;
        using var aes=Aes.Create();aes.Key=Convert.FromHexString("40178130DF0A94543309E171ECBF254C");aes.Mode=CipherMode.ECB;aes.Padding=PaddingMode.None;
        using var encrypt=aes.CreateEncryptor();byte[] clear=new byte[bytes.Length-32],block=new byte[16];
        for(int i=0;i<clear.Length;i+=16){encrypt.TransformBlock(counter,0,16,block,0);for(int j=0;j<16 && i+j<clear.Length;j++)clear[i+j]=(byte)(bytes[32+i+j]^block[j]);for(int j=15;j>=0 && ++counter[j]==0;j--){}}
        Need(DCX.Is(clear) || BND4.Is(clear),"Regulation decryption failed");bytes=clear;
    }
    var expanded=Expand(bytes,out _);
    if(BND4.Is(expanded)){var binder=BND4.Read(expanded);binder.Compression=DCX.Type.None;return binder.Write();}
    return expanded;
}
sealed class Original : IDisposable {
    readonly string game;readonly ILookup<ulong,BHD5.FileHeader> files;readonly FileStream data;
    public Original(string path){
        game=Path.GetFullPath(path);Need(Hash(File.ReadAllBytes(Path.Combine(game,"DarkSoulsII.exe")))==GameHash,"Unsupported Scholar executable");
        using var rsa=RSA.Create();rsa.ImportFromPem(File.ReadAllText(Path.Combine(game,"GameDataKeyCode.pem")));var parameters=rsa.ExportParameters(false);
        var modulus=new BigInteger(parameters.Modulus!,true,true);var exponent=new BigInteger(parameters.Exponent!,true,true);int blockSize=parameters.Modulus!.Length;
        using var clear=new MemoryStream();using(var input=File.OpenRead(Path.Combine(game,"GameDataEbl.bhd"))){
            byte[] block=new byte[blockSize];int count;
            while((count=input.Read(block))!=0){Need(count==block.Length,"Partial BHD block");var decoded=BigInteger.ModPow(new BigInteger(block,true,true),exponent,modulus).ToByteArray(true,true);var padded=new byte[blockSize-1];Need(decoded.Length<=padded.Length,"Invalid decoded BHD block");decoded.CopyTo(padded,padded.Length-decoded.Length);clear.Write(padded);}
        }
        clear.Position=0;files=BHD5.Read(clear,BHD5.Game.DarkSouls2).Buckets.SelectMany(x=>x).ToLookup(x=>x.FileNameHash);
        data=File.OpenRead(Path.Combine(game,"GameDataEbl.bdt"));
    }
    public byte[] Read(string key){
        if(key=="enc_regulation.bnd.dcx")return File.ReadAllBytes(Path.Combine(game,key));
        if(key=="sfx/sfx9999.ffxbnd.dcx")return File.ReadAllBytes(Safe(game,key));
        if(key=="menu/tex/icon/ic_0060425001.tpf")return []; // Original authored Lantern icon.
        uint hash=("/"+key).ToLowerInvariant().Aggregate(0u,(h,c)=>unchecked(h*37+c));var found=files[hash].ToArray();Need(found.Length==1,"Missing/ambiguous original: "+key);return found[0].ReadFile(data);
    }
    public void Dispose()=>data.Dispose();
}
static void Delta(byte[] source,byte[] target,string path){
    var index=new Dictionary<ulong,List<int>>();
    for(int i=0;i+32<=source.Length;i+=32){var hash=BinaryPrimitives.ReadUInt64LittleEndian(source.AsSpan(i,8));if(!index.TryGetValue(hash,out var offsets))index[hash]=offsets=[];if(offsets.Count<16)offsets.Add(i);}
    using var output=new BinaryWriter(new GZipStream(File.Create(path),CompressionLevel.SmallestSize));output.Write("DS2FX-DELTA-1");output.Write(target.Length);
    int position=0,start=0;
    void Literal(int end){if(end>start){output.Write((byte)2);output.Write(end-start);output.Write(target,start,end-start);}}
    while(position<target.Length){
        int match=-1;
        if(position+32<=target.Length && index.TryGetValue(BinaryPrimitives.ReadUInt64LittleEndian(target.AsSpan(position,8)),out var candidates))
            foreach(int offset in candidates)if(target.AsSpan(position,32).SequenceEqual(source.AsSpan(offset,32))){match=offset;break;}
        if(match<0){position++;continue;}
        Literal(position);int length=32;while(position+length<target.Length && match+length<source.Length && target[position+length]==source[match+length])length++;
        output.Write((byte)1);output.Write(match);output.Write(length);position+=length;start=position;
    }
    Literal(position);output.Write((byte)0);
}
static byte[] Patch(byte[] source,string path){
    using var input=new BinaryReader(new GZipStream(File.OpenRead(path),CompressionMode.Decompress));Need(input.ReadString()=="DS2FX-DELTA-1","Bad delta header");int size=input.ReadInt32();Need(size>=0 && size<=512*1024*1024,"Invalid delta size");
    using var output=new MemoryStream(size);byte op;
    while((op=input.ReadByte())!=0){
        if(op==1){int offset=input.ReadInt32(),length=input.ReadInt32();Need(offset>=0 && length>0 && (long)offset+length<=source.Length && output.Length+length<=size,"Invalid copy range");output.Write(source,offset,length);}
        else {Need(op==2,"Unknown delta opcode");int length=input.ReadInt32();Need(length>0 && output.Length+length<=size,"Invalid literal range");var bytes=input.ReadBytes(length);Need(bytes.Length==length,"Truncated delta");output.Write(bytes);}
    }
    Need(output.Length==size,"Wrong reconstructed size");return output.ToArray();
}
static int Main(string[] args){try {
    Need(args.Length==4,"Usage: StandaloneAssets create <Game folder> <Lantern content folder> <fresh patch folder> OR build <Game folder> <patch folder> <fresh output assets folder>");
    using var original=new Original(args[1]);
    string input=Path.GetFullPath(args[2]),output=Path.GetFullPath(args[3]);
    Need(!Directory.Exists(output),"Output must be a fresh folder");
    Need(!output.StartsWith(Path.GetFullPath(args[1])+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase),"Build outside the game folder");Directory.CreateDirectory(output);
    if(args[0]=="hud"){
        var manifest=new List<string>{"DS2FX-HUD-ASSETS-1"};
        var keys=File.ReadAllLines(input).Where(x=>x.StartsWith("/menu/tex/icon/vow/vi_",StringComparison.Ordinal) || x.StartsWith("/menu/tex/icon/effect/ei_",StringComparison.Ordinal)).Distinct().Order().ToArray();
        Need(keys.Length>=10 && keys.Length<=512,"Invalid icon dictionary");
        foreach(var key in keys){
            string name=Path.GetFileNameWithoutExtension(key);Need(name.Length==7 && name[3..].All(char.IsDigit),"Invalid icon name");
            var texture=TPF.Read(original.Read(key.TrimStart('/')));Need(texture.Platform==TPF.TPFPlatform.PC && texture.Textures.Count==1 && string.Equals(texture.Textures[0].Name,name,StringComparison.OrdinalIgnoreCase),"Invalid icon texture: "+name);
            string dds=Path.Combine(output,name+".dds"),png=Path.Combine(output,name+".png");File.WriteAllBytes(dds,texture.Textures[0].Bytes);
            var start=new System.Diagnostics.ProcessStartInfo(Path.Combine(AppContext.BaseDirectory,"DS2FX-IconConverter.exe")){UseShellExecute=false,CreateNoWindow=true};start.ArgumentList.Add(dds);start.ArgumentList.Add(png);
            using var child=System.Diagnostics.Process.Start(start)!;child.WaitForExit();Need(child.ExitCode==0,"Icon decoding failed: "+name);File.Delete(dds);
            manifest.Add(name+" "+Hash(File.ReadAllBytes(png)));
        }
        File.WriteAllLines(Path.Combine(output,"icons.tsv"),manifest);Console.WriteLine($"Built {keys.Length} original covenant/status icons locally. Do not redistribute this game-derived cache.");return 0;
    }
    if(args[0]=="create"){
        var assets=new List<Asset>();
        foreach(var row in File.ReadAllLines(Path.Combine(input,"files.tsv")).Skip(1)){
            var parts=row.Split('\t');Need(parts.Length==2,"Invalid content row");string key=parts[1];var source=original.Read(key);var target=File.ReadAllBytes(Safe(Path.Combine(input,"assets"),key));Need(Hash(target)==parts[0],"Target manifest changed");
            var sourceClear=ExpandSource(key,source);var targetClear=Expand(target,out int dcx);string patch=assets.Count+".delta.gz";Delta(sourceClear,targetClear,Path.Combine(output,patch));
            var check=Patch(sourceClear,Path.Combine(output,patch));if(dcx>=0)check=DCX.Compress(check,(DCX.Type)dcx);Need(Hash(check)==parts[0],"Delta roundtrip/recompression failed: "+key);
            assets.Add(new(key,Hash(source),Hash(target),dcx,patch,target.Length));Console.WriteLine($"Verified {key}: {new FileInfo(Path.Combine(output,patch)).Length:N0} patch bytes / {target.Length:N0} target bytes");
        }
        File.WriteAllText(Path.Combine(output,"assets.json"),JsonSerializer.Serialize(assets,new JsonSerializerOptions{WriteIndented=true}));
    }else {
        Need(args[0]=="build","Unknown operation");var assets=JsonSerializer.Deserialize<Asset[]>(File.ReadAllText(Path.Combine(input,"assets.json")))!;
        var staged=new List<(string path,byte[] data)>();
        foreach(var asset in assets){var source=original.Read(asset.Key);Need(Hash(source)==asset.SourceHash,"Original file changed: "+asset.Key);var bytes=Patch(ExpandSource(asset.Key,source),Safe(input,asset.Patch));if(asset.TargetDcx>=0)bytes=DCX.Compress(bytes,(DCX.Type)asset.TargetDcx);Need(bytes.LongLength==asset.Size && Hash(bytes)==asset.TargetHash,"Generated file hash mismatch: "+asset.Key);staged.Add((Safe(output,asset.Key),bytes));}
        foreach(var file in staged){Directory.CreateDirectory(Path.GetDirectoryName(file.path)!);File.WriteAllBytes(file.path,file.data);}Console.WriteLine($"Built and hash-verified {staged.Count} standalone Lantern assets. Game files unchanged.");
    }
    return 0;
}catch(Exception error){Console.Error.WriteLine(error.Message);return 1;}}
}
