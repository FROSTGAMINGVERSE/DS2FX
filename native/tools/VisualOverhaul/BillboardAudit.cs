using SoulsFormats;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;

// Inventory all supplied static alpha billboards, not only previously shortlisted objects.
static class BillboardAudit {
    const string Prefix="SeekerOfFire_Seamless/";
    static readonly JsonSerializerOptions Json=new(){IncludeFields=true};
    static byte[] Bytes(ZipArchiveEntry e){using var s=e.Open();using var m=new MemoryStream();s.CopyTo(m);return m.ToArray();}
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
    static string NonVisual(MSB2.Part.Object p){var j=JsonSerializer.SerializeToElement(p,Json);return string.Join('|',j.EnumerateObject().Where(f=>f.Name is not ("Name" or "ModelName" or "Position" or "Rotation" or "Scale" or "DrawGroups" or "DispGroups")).Select(f=>f.Name+":"+f.Value.GetRawText()));}
    public static void Run(ZipArchive zip,GameArchive game,string output){
        Directory.CreateDirectory(Path.Combine(output,"textures"));var found=new Dictionary<string,object>();int scanned=0;
        foreach(var e in zip.Entries.Where(e=>e.FullName.StartsWith(Prefix+"model/obj/")&&e.FullName.EndsWith(".bnd"))){
            scanned++;var bytes=Bytes(e);if(!BND4.Is(bytes))continue;var b=BND4.Read(bytes);var flvs=b.Files.Where(f=>f.Name.EndsWith(".flv")||f.Name.EndsWith(".flv.dcx")).ToArray();if(flvs.Length!=1)continue;
            var mesh=FLVER2.Read(flvs[0].Bytes);if(mesh.Meshes.Sum(m=>m.Vertices.Count)!=4||mesh.Materials.Count!=1||!mesh.Materials[0].MTD.Contains("NoLit",StringComparison.OrdinalIgnoreCase)||mesh.Meshes.Any(m=>m.Dynamic!=0))continue;
            string id=Path.GetFileNameWithoutExtension(e.FullName);var textures=new List<object>();
            foreach(var f in b.Files.Where(f=>f.Name.EndsWith(".tpf")||f.Name.EndsWith(".tpf.dcx"))){var t=TPF.Read(f.Bytes);int index=0;foreach(var texture in t.Textures){var file=$"{id}-{index++}-{texture.Name}.dds";File.WriteAllBytes(Path.Combine(output,"textures",file),texture.Bytes);textures.Add(new{texture.Name,file,bytes=texture.Bytes.Length,sha256=Hash(texture.Bytes)});}}
            byte[]? original=null;try{original=game.Read(e.FullName[Prefix.Length..]);}catch(FileNotFoundException){}
            found[id]=new{id,sha256=Hash(bytes),original_exists=original!=null,original_sha256=original==null?null:Hash(original),files=b.Files.Select(f=>new{f.Name,bytes=f.Bytes.Length,sha256=Hash(f.Bytes),decompressed_hex=f.Name.Contains(".mte")&&DCX.Decompress(f.Bytes).Length<=32?Convert.ToHexString(DCX.Decompress(f.Bytes)):null}),material=mesh.Materials.Single(),textures};
        }
        var placements=new List<object>();
        foreach(var e in zip.Entries.Where(e=>e.FullName.StartsWith(Prefix+"map/")&&e.FullName.EndsWith(".msb"))){
            string id=Path.GetFileNameWithoutExtension(e.FullName);var source=MSB2.Read(Bytes(e));var original=MSB2.Read(game.Read(e.FullName[Prefix.Length..]));
            foreach(var p in source.Parts.Objects.Where(p=>found.ContainsKey(p.ModelName))){
                var before=original.Parts.Objects.SingleOrDefault(o=>o.Name==p.Name);var zeroFlag=(MSB2.Part.Object)p.DeepCopy();zeroFlag.UnkT04=0;
                placements.Add(new{map=id,p.Name,p.ModelName,p.MapObjectInstanceParamID,p.UnkT04,p.Position,p.Rotation,p.Scale,p.DrawGroups,p.DispGroups,source_pose=source.PartPoses.Any(q=>q.PartName==p.Name),original_name_present=before!=null,original=before,
                    exact_nonvisual_templates=original.Parts.Objects.Where(o=>o.MapObjectInstanceParamID==0&&o.UnkT04==0&&!original.PartPoses.Any(q=>q.PartName==o.Name)&&NonVisual(o)==NonVisual(p)).Select(o=>new{o.Name,o.ModelName}),
                    zero_flag_nonvisual_templates=original.Parts.Objects.Where(o=>o.MapObjectInstanceParamID==0&&o.UnkT04==0&&!original.PartPoses.Any(q=>q.PartName==o.Name)&&NonVisual(o)==NonVisual(zeroFlag)).Select(o=>new{o.Name,o.ModelName})});
            }
        }
        File.WriteAllText(Path.Combine(output,"billboard-audit.json"),JsonSerializer.Serialize(new{scanned_binders=scanned,models=found.Values,placements,scope="Read-only inventory; images exported for visual identification. Template matches alone do not certify gameplay or live visibility."},new JsonSerializerOptions{IncludeFields=true,WriteIndented=true}));
        Console.WriteLine($"Scanned {scanned} supplied binders; found {found.Count} four-vertex unlit billboards and {placements.Count} placements.");
    }
}
