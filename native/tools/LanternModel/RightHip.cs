using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
static class RightHip {
 static void Need(bool v,string s){if(!v)throw new InvalidDataException(s);}
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static Matrix4x4 World(FLVER2 f,int i)=>f.Bones[i].ComputeLocalTransform()*(f.Bones[i].ParentIndex<0?Matrix4x4.Identity:World(f,f.Bones[i].ParentIndex));
 static Vector3 Point(FLVER2 f,string name)=>Vector3.Transform(Vector3.Zero,World(f,f.Bones.FindIndex(b=>b.Name==name)));
 public static void Run(string input,string output){
 Need(!File.Exists(output),"Fresh output required");var bytes=File.ReadAllBytes(input);Need(Hash(bytes)=="D68138D26724658633206AF763C39516D2A163F8646EC67E2D93695DEA79A143","Unsupported model");
 var b=BND4.Read(bytes);var e=b.Files.Single(x=>x.Name=="c0001.flv");var f=FLVER2.Read(e.Bytes);var baseline=f.Write();
 Need(f.Bones.Count==74&&f.Bones[73].Name=="DS2FX_LanternPivot"&&f.Bones[73].ParentIndex==3&&f.Meshes.Count==3&&f.Meshes.All(m=>m.BoneIndices.SequenceEqual(new[]{73}))&&f.Materials.All(m=>m.Name.StartsWith("DS2FX_Lantern_")),"Owned model mismatch");
 var d=f.Dummies.Single(x=>x.ReferenceID==247);Need(d.ParentBoneIndex==73&&d.AttachBoneIndex==-1,"Wick attachment mismatch");
 var right=Point(f,"R_Hand");var left=Point(f,"L_Hand");var forward=Point(f,"R_Toe0")-Point(f,"R_Foot");
 Need(Math.Abs(right.X-left.X)>.3f&&Math.Abs(forward.Z)>.02f,"Ambiguous model axes");
 var old=f.Bones[73].Translation;var oldWorld=Vector3.Transform(Vector3.Zero,World(f,73));var target=new Vector3(Math.Sign(right.X-left.X)*.24f,oldWorld.Y,-Math.Sign(forward.Z)*.16f);var delta=target-oldWorld;
 Need(Matrix4x4.Invert(World(f,3),out var inv),"Invalid pelvis");f.Bones[73].Translation=Vector3.Transform(target,inv);
 var positions=f.Meshes.Select(m=>m.Vertices.Select(v=>v.Position).ToArray()).ToArray();
 var oldMin=f.Header.BoundingBoxMin;var oldMax=f.Header.BoundingBoxMax;
 foreach(var m in f.Meshes)foreach(var v in m.Vertices)v.Position+=delta;
 f.Header.BoundingBoxMin=f.Meshes.SelectMany(m=>m.Vertices).Select(v=>v.Position).Aggregate(Vector3.Min);f.Header.BoundingBoxMax=f.Meshes.SelectMany(m=>m.Vertices).Select(v=>v.Position).Aggregate(Vector3.Max);
 // Bone-local geometry, bounds, and the wick offset remain unchanged because the whole assembly translates.
 e.Bytes=f.Write();var verify=FLVER2.Read(e.Bytes);var wick=Vector3.Transform(verify.Dummies.Single(x=>x.ReferenceID==247).Position,World(verify,73));
 var wm=verify.Meshes.Single(m=>verify.Materials[m.MaterialIndex].Name=="DS2FX_Lantern_4");var min=wm.Vertices.Select(v=>v.Position).Aggregate(Vector3.Min);var max=wm.Vertices.Select(v=>v.Position).Aggregate(Vector3.Max);var top=new Vector3((min.X+max.X)/2,max.Y,(min.Z+max.Z)/2);
 Need(Vector3.Distance(top,wick)<.00001f,"Flame anchor no longer centered on wick");
 verify.Bones[73].Translation=old;verify.Header.BoundingBoxMin=oldMin;verify.Header.BoundingBoxMax=oldMax;for(int i=0;i<verify.Meshes.Count;i++)for(int j=0;j<verify.Meshes[i].Vertices.Count;j++)verify.Meshes[i].Vertices[j].Position=positions[i][j];Need(verify.Write().SequenceEqual(baseline),"Undeclared model change");
 Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);b.Write(output);var check=BND4.Read(output);var original=BND4.Read(bytes);for(int i=0;i<check.Files.Count;i++){var a=check.Files[i];var o=original.Files[i];Need(a.ID==o.ID&&a.Name==o.Name&&a.Flags==o.Flags&&a.CompressionType==o.CompressionType,"Binder identity changed");if(a.Name!=e.Name)Need(a.Bytes.SequenceEqual(o.Bytes),"Companion changed");}
 var t=f.Bones[73].Translation;var audit=new{Before=Hash(bytes),After=Hash(File.ReadAllBytes(output)),RightHand=new[]{right.X,right.Y,right.Z},Forward=new[]{forward.X,forward.Y,forward.Z},Target=new[]{target.X,target.Y,target.Z},Translation=new[]{t.X,t.Y,t.Z},WickError=Vector3.Distance(top,wick),OriginalRestoredExactly=true,LiveValidated=false};File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine(JsonSerializer.Serialize(audit));
 }
}
