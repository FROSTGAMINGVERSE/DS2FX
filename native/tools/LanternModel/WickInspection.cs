// Read-only validation of the installed owned model's shared animated wick.
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
static class WickInspection {
    static Matrix4x4 World(FLVER2 f,int i)=>f.Bones[i].ComputeLocalTransform()*(f.Bones[i].ParentIndex<0?Matrix4x4.Identity:World(f,f.Bones[i].ParentIndex));
    public static void Run(string input) {
        var bytes=File.ReadAllBytes(input);
        if(Convert.ToHexString(SHA256.HashData(bytes))!="D68138D26724658633206AF763C39516D2A163F8646EC67E2D93695DEA79A143")throw new InvalidDataException("Unsupported owned base Lantern model");
        var b=BND4.Read(bytes);var f=FLVER2.Read(b.Files.Single(x=>x.Name=="c0001.flv").Bytes);
        var d=f.Dummies.Single(x=>x.ReferenceID==247);
        if(f.Bones.Count!=74||d.ParentBoneIndex!=73||d.AttachBoneIndex!=-1||f.Bones[73].Name!="DS2FX_LanternPivot"||f.Bones[73].ParentIndex!=3||f.Bones[3].Name!="Pelvis")throw new InvalidDataException("Shared wick/pivot layout mismatch");
        if(f.Meshes.Count!=3||f.Meshes.Any(m=>!m.BoneIndices.SequenceEqual(new[]{73})))throw new InvalidDataException("Lamp geometry does not share wick pivot");
        var wick=Vector3.Transform(d.Position,World(f,73));var expected=new Vector3(.27f,.822f,-.055f);var error=Vector3.Distance(wick,expected);
        if(error>0.00001f)throw new InvalidDataException("Wick bind point moved");
        var wickMesh=f.Meshes.Single(m=>f.Materials[m.MaterialIndex].Name=="DS2FX_Lantern_4");
        var min=wickMesh.Vertices.Select(v=>v.Position).Aggregate(Vector3.Min);var max=wickMesh.Vertices.Select(v=>v.Position).Aggregate(Vector3.Max);
        var wickTop=new Vector3((min.X+max.X)/2,max.Y,(min.Z+max.Z)/2);var geometryError=Vector3.Distance(wick,wickTop);
        if(geometryError>0.00001f)throw new InvalidDataException("Anchor is not at centered wick top");
        Console.WriteLine(JsonSerializer.Serialize(new {GeometryWickTopErrorMetres=geometryError,WickMeshBounds=new{Min=new[]{min.X,min.Y,min.Z},Max=new[]{max.X,max.Y,max.Z}},ModelSha256=Convert.ToHexString(SHA256.HashData(bytes)),Dummy=247,Bone=73,Name=f.Bones[73].Name,SharedWithOwnedMeshes=3,StoredPoint=new[]{d.Position.X,d.Position.Y,d.Position.Z},BindWorldPoint=new[]{wick.X,wick.Y,wick.Z},BindErrorMetres=error,LiveFlameContainmentVerified=false},new JsonSerializerOptions{WriteIndented=true}));
    }
}
