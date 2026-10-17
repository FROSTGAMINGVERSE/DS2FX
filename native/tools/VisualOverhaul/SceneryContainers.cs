using SoulsFormats;
using System.IO.Compression;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

// Read-only structural comparison of the separately excluded environment containers.
static class SceneryContainers {
    const string Prefix="SeekerOfFire_Seamless/";
    static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
    static byte[] Bytes(ZipArchiveEntry e){using var s=e.Open();using var m=new MemoryStream();s.CopyTo(m);return m.ToArray();}
    public static void Run(ZipArchive zip,GameArchive game,string output){
        var reports=new List<object>();
        foreach(var e in zip.Entries.Where(e=>e.FullName.StartsWith(Prefix+"model_lq/map/")&&e.FullName.EndsWith(".gibhd")||e.FullName.StartsWith(Prefix+"model/map/")&&e.FullName.EndsWith(".mapbhd")).OrderBy(e=>e.FullName)){
            string relative=e.FullName[Prefix.Length..];var header=Bytes(e);byte[]? originalHeader=null;
            try{originalHeader=game.Read(relative);}catch(FileNotFoundException){}
            // Only the three changed main map headers, plus all six LQ archive pairs.
            if(relative.StartsWith("model/map/")&&originalHeader!=null&&Hash(header)==Hash(originalHeader))continue;
            string dataPath=relative[..^2]+"dt";var dataEntry=zip.GetEntry(Prefix+dataPath)??throw new FileNotFoundException(dataPath);
            var data=Bytes(dataEntry);byte[]? originalData=null;
            try{originalData=game.Read(dataPath);}catch(FileNotFoundException){}
            using var source=new BXF4Reader(header,data);using var original=originalHeader!=null&&originalData!=null?new BXF4Reader(originalHeader,originalData):null;
            var old=original?.Files.ToDictionary(f=>f.Name,StringComparer.Ordinal)??new();var current=source.Files.Select(f=>f.Name).ToHashSet(StringComparer.Ordinal);
            var entries=new List<object>();
            foreach(var f in source.Files){
                var bytes=source.ReadFile(f);byte[]? baseline=null;if(old.TryGetValue(f.Name,out var before))baseline=original!.ReadFile(before);
                bool same=baseline!=null&&Hash(bytes)==Hash(baseline);object? geometry=null;
                if(!same&&(f.Name.EndsWith(".flv",StringComparison.OrdinalIgnoreCase)||f.Name.EndsWith(".flv.dcx",StringComparison.OrdinalIgnoreCase))){
                    var mesh=FLVER2.Read(bytes);var vertices=mesh.Meshes.SelectMany(m=>m.Vertices).Select(v=>v.Position).ToArray();
                    var min=vertices.Length==0?Vector3.Zero:vertices.Aggregate(new Vector3(float.PositiveInfinity),Vector3.Min);var max=vertices.Length==0?Vector3.Zero:vertices.Aggregate(new Vector3(float.NegativeInfinity),Vector3.Max);
                    geometry=new{vertices=vertices.Length,min,max,size=max-min,materials=mesh.Materials.Select(m=>new{m.Name,m.MTD,texture_paths=m.Textures.Select(t=>t.Path)})};
                }
                entries.Add(new{f.Name,f.ID,f.Flags,bytes=bytes.Length,source_sha256=Hash(bytes),vanilla_sha256=baseline==null?null:Hash(baseline),status=original==null?"baseline-unavailable":baseline==null?"added":same?"identical":"changed",geometry});
            }
            reports.Add(new{relative,data_path=dataPath,source_header_sha256=Hash(header),vanilla_header_sha256=originalHeader==null?null:Hash(originalHeader),source_data_sha256=Hash(data),vanilla_data_sha256=originalData==null?null:Hash(originalData),entries,removed=original?.Files.Where(f=>!current.Contains(f.Name)).Select(f=>new{f.Name,f.ID}),scope="Read-only archive-entry comparison; missing canonical baseline is explicit. No automatic inclusion or runtime LOD/lighting certification."});
        }
        File.WriteAllText(Path.Combine(output,"environment-containers.json"),JsonSerializer.Serialize(reports,new JsonSerializerOptions{WriteIndented=true,IncludeFields=true}));
        Console.WriteLine($"Compared {reports.Count} excluded environment containers without deploying them.");
    }
}
