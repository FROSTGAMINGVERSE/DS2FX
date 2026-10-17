using SoulsFormats;
using System.Security.Cryptography;
using System.Text.Json;

// Compile an owned depleted icon for native quick-slot presentation. No item
// definition, regulation, inventory quantity, Oil route or Estus data changes.
static class LanternPresentation
{
    static string Hash(byte[] data)=>Convert.ToHexString(SHA256.HashData(data));
    static void Require(bool ok,string reason){if(!ok)throw new InvalidDataException(reason);}
    static readonly Dictionary<string,string> Regulations=new(){
        ["assets/enc_regulation.bnd.dcx"]="94BE28F184793F9B5B17EC3E01753E5DCA9F49CF1DF8CB159F099603771E326C",
        ["variants/standard-jump.bnd.dcx"]="AE16A3AA5BFFDEBACAA4CE4B7D23B9C13BBB4AA0CDC7B7BC4EC02A41983A4130",
        ["variants/estus-off-jump-on.bnd.dcx"]="BBEDABE07B5479C8743B9B2EFA291300A0DA716098DEBC0B5C6F1C528DD2BC83",
        ["variants/estus-off-jump-off.bnd.dcx"]="7D8A8B755DD007297CA1C93605A930AA73D0DC97E2244442400F79DC156C3272"};
    static ushort Grey(ushort rgb) {
        int r=(rgb>>11)*255/31,g=((rgb>>5)&63)*255/63,b=(rgb&31)*255/31;
        int luma=((54*r+183*g+19*b+128)>>8)*3/4;
        return (ushort)(((luma*31/255)<<11)|((luma*63/255)<<5)|(luma*31/255));
    }
    internal static void Build(string input,string output) {
        input=Path.GetFullPath(input);output=Path.GetFullPath(output);
        Require(!Directory.Exists(output)&&!output.StartsWith(input,StringComparison.OrdinalIgnoreCase),"Fresh external candidate required");
        foreach(var pair in Regulations)Require(Hash(File.ReadAllBytes(Path.Combine(input,pair.Key)))==pair.Value,"Unknown regulation baseline: "+pair.Key);
        const string sourceKey="menu/tex/icon/ic_0060425001.tpf",emptyKey="menu/tex/icon/ic_0060425002.tpf";
        var source=File.ReadAllBytes(Path.Combine(input,"assets",sourceKey));
        Require(Hash(source)=="FE946E2B83C2C0517B5F285097591F011F8184529AEAE74B6EF1CF8F2A8A37FA","Unknown owned Lantern icon");
        var t=TPF.Read(source);Require(t.Textures.Count==1&&t.Textures[0].Name=="IT_IC_0060425001","Unexpected Lantern icon identity");
        var original=t.Textures[0].Bytes.ToArray();var bytes=original.ToArray();
        Require(bytes.Length==8320&&System.Text.Encoding.ASCII.GetString(bytes,0,4)=="DDS "&&System.Text.Encoding.ASCII.GetString(bytes,84,4)=="DXT5"&&BitConverter.ToInt32(bytes,12)==128&&BitConverter.ToInt32(bytes,16)==64,"Expected one-mip 64x128 BC3 native DDS");
        // BC3 endpoint conversion preserves every alpha block and interpolation
        // index. No image-generation replacement or added artwork/text.
        for(int off=128;off<bytes.Length;off+=16)for(int c=0;c<2;++c)BitConverter.GetBytes(Grey(BitConverter.ToUInt16(bytes,off+8+c*2))).CopyTo(bytes,off+8+c*2);
        for(int n=0;n<bytes.Length;++n)Require(n>=128&&(n-128)%16 is >=8 and <12||bytes[n]==original[n],"Unexpected DDS alpha/header/index edit");
        t.Textures[0].Name="IT_IC_0060425002";t.Textures[0].Bytes=bytes;var empty=t.Write();
        var verify=TPF.Read(empty);Require(verify.Textures.Single().Bytes.SequenceEqual(bytes)&&verify.Textures.Single().Name=="IT_IC_0060425002","Depleted icon round-trip failed");
        Directory.CreateDirectory(output);
        var before=Directory.GetFiles(input,"*",SearchOption.AllDirectories).ToDictionary(f=>Path.GetRelativePath(input,f),f=>Hash(File.ReadAllBytes(f)));
        foreach(var file in before.Keys){var target=Path.Combine(output,file);Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.Copy(Path.Combine(input,file),target);}
        File.WriteAllBytes(Path.Combine(output,"assets",emptyKey),empty);
        var manifest=Path.Combine(output,"files.tsv");var lines=File.ReadAllLines(manifest);
        Require(lines.Skip(1).All(l=>l.Split('\t')[1]!=emptyKey),"Depleted icon route collision");
        File.WriteAllLines(manifest,lines.Concat(new[]{Hash(empty).ToLowerInvariant()+"\t"+emptyKey}));
        File.AppendAllLines(Path.Combine(output,"forge.ini"),new[]{"LanternFlameDriver=charged-toggle-native-sfx-handle","LanternChargeDisplay=native-quick-slot","LanternDepletedIcon=60425002"});
        foreach(var pair in before)Require(pair.Key is "files.tsv" or "forge.ini"||Hash(File.ReadAllBytes(Path.Combine(output,pair.Key)))==pair.Value,"Unrelated pack file changed: "+pair.Key);
        foreach(var pair in Regulations)Require(Hash(File.ReadAllBytes(Path.Combine(output,pair.Key)))==pair.Value,"Regulation changed");
        File.WriteAllText(Path.Combine(output,"lantern-presentation.audit.json"),JsonSerializer.Serialize(new{Role="offline-native-presentation-candidate",RegulationsByteIdentical=Regulations,SourceIcon=Hash(source),DepletedIcon=Hash(empty),NativeIcon=60425001,DepletedNativeIcon=60425002,AlphaHeaderAndIndicesPreserved=true,GreyscaleLuminanceScale=0.75,OilReintroduced=false,EstusUntouched=true,LiveVerified=false},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine("Native depleted Lantern icon compiled; all four regulations and existing assets byte-identical.");
    }
}
