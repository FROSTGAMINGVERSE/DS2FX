// SPDX-License-Identifier: GPL-3.0-or-later
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;
static class BakedRefresh {
 static void Need(bool v,string s){if(!v)throw new InvalidDataException(s);}
 static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
 static Vector3 V(JsonElement a)=>new(a[0].GetSingle(),a[1].GetSingle(),a[2].GetSingle());
 static Matrix4x4 World(FLVER2 f,int i)=>f.Bones[i].ComputeLocalTransform()*(f.Bones[i].ParentIndex<0?Matrix4x4.Identity:World(f,f.Bones[i].ParentIndex));
 public static void Run(string input,string geometry,string textures,string output){
  Need(!File.Exists(output),"Fresh output required");var bytes=File.ReadAllBytes(input);
  Need(Hash(bytes)=="BC2AB2B0162BD43C6E9B36B4B55973F1B58C25F788760B0EAD3DC74F63390B8B","Unsupported right-rear source");
  var binder=BND4.Read(bytes);var entry=binder.Files.Single(x=>x.Name=="c0001.flv");var f=FLVER2.Read(entry.Bytes);var baseline=f.Write();
  Need(f.Bones.Count==74&&f.Bones[73].Name=="DS2FX_LanternPivot"&&f.Bones[73].ParentIndex==3&&f.Bones[73].ChildIndex==-1,"Owned leaf mismatch");
  Need(f.Meshes.Count==3&&f.Materials.Count==3&&f.Meshes.All(m=>m.DefaultBoneIndex==73&&m.BoneIndices.SequenceEqual(new[]{73})),"Owned meshes mismatch");
  var wick=f.Dummies.Single(d=>d.ReferenceID==247);Need(wick.ParentBoneIndex==73,"Wick mismatch");
  var oldMeshes=f.Meshes.ToArray();var oldMin=f.Header.BoundingBoxMin;var oldMax=f.Header.BoundingBoxMax;
  var pivot=Vector3.Transform(Vector3.Zero,World(f,73));
  using var doc=JsonDocument.Parse(File.ReadAllText(geometry));Need(doc.RootElement.GetProperty("open_windows").GetBoolean(),"Closed cage");
  f.Meshes.Clear();int triangles=0;
  foreach(var group in doc.RootElement.GetProperty("groups").EnumerateArray()){
   int slot=group.GetProperty("material").GetInt32();Need(slot is 0 or 1 or 4,"Unknown material slot");
   int mi=f.Materials.FindIndex(m=>m.Name=="DS2FX_Lantern_"+slot);Need(mi>=0,"Missing native material");var old=oldMeshes.Single(m=>m.MaterialIndex==mi);
   var mesh=new FLVER2.Mesh{Dynamic=old.Dynamic,MaterialIndex=mi,DefaultBoneIndex=73};mesh.BoneIndices.Add(73);
   foreach(var vb in old.VertexBuffers)mesh.VertexBuffers.Add(new FLVER2.VertexBuffer(vb.LayoutIndex));
   var indices=new List<int>();var prototype=old.Vertices[0];
   foreach(var tri in group.GetProperty("triangles").EnumerateArray()){
    Need(tri.GetArrayLength()==3,"Non-triangle geometry");triangles++;
    foreach(var vertex in tri.EnumerateArray()){
     var v=new FLVER.Vertex(prototype);var p=V(vertex.GetProperty("p"));Need(p.Length()<.29f,"Geometry escaped assembly bounds");v.Position=p+pivot;v.Normal=Vector3.Normalize(V(vertex.GetProperty("n")));
     var t=vertex.GetProperty("t");v.Tangents[0]=new Vector4(Vector3.Normalize(V(t)),t[3].GetSingle());var uv=vertex.GetProperty("uv");v.UVs[0]=new(uv[0].GetSingle(),uv[1].GetSingle(),0);
     v.BoneIndices[0]=0;v.BoneWeights[0]=1;for(int k=1;k<4;k++){v.BoneIndices[k]=0;v.BoneWeights[k]=0;}
     // Float overload uses normalized channels; integer literals select the
     // byte overload and turn white into 1/255, darkening the native material.
     v.Colors[0]=new FLVER.VertexColor(1f,1f,1f,1f);indices.Add(mesh.Vertices.Count);mesh.Vertices.Add(v);
    }
   }
   foreach(var fs in old.FaceSets)mesh.FaceSets.Add(new FLVER2.FaceSet(fs.Flags,false,fs.CullBackfaces,fs.Unk06,new List<int>(indices)));
   f.Meshes.Add(mesh);
   foreach(var suffix in new[]{"d","s","n"}){
    var name=$"DS2FX_Lantern_{slot}_{suffix}";var tf=binder.Files.Single(x=>x.Name==name+".tpf");var tpf=TPF.Read(tf.Bytes);Need(tpf.Textures.Count==1&&tpf.Textures[0].Name==name,"Texture identity mismatch");
    var dds=File.ReadAllBytes(Path.Combine(textures,name+".dds"));Need(dds.Length>128,"Truncated DDS");
    var fourcc=System.Text.Encoding.ASCII.GetString(dds,84,4);
    var argb=BitConverter.ToUInt32(dds,84)==0&&BitConverter.ToUInt32(dds,88)==32&&BitConverter.ToUInt32(dds,92)==0x00ff0000&&BitConverter.ToUInt32(dds,104)==0xff000000;
    Need(fourcc=="DXT5"||argb,"Expected legacy BC3 or ARGB32 DDS");
    tpf.Textures[0].Format=(byte)(argb?9:5);tpf.Textures[0].Bytes=dds;tf.Bytes=tpf.Write();Need(TPF.Read(tf.Bytes).Textures.Single().Bytes.SequenceEqual(dds),"Texture roundtrip failed");
   }
  }
  Need(triangles==6420&&f.Meshes.Count==3,"Unexpected v4 mesh count");
  f.Header.BoundingBoxMin=f.Meshes.SelectMany(m=>m.Vertices).Select(v=>v.Position).Aggregate(Vector3.Min);f.Header.BoundingBoxMax=f.Meshes.SelectMany(m=>m.Vertices).Select(v=>v.Position).Aggregate(Vector3.Max);
  entry.Bytes=f.Write();var check=FLVER2.Read(entry.Bytes);
  Need(check.Meshes.All(m=>m.BoneIndices.SequenceEqual(new[]{73})&&m.Vertices.All(v=>v.BoneWeights[0]==1&&v.BoneIndices[0]==0)),"Skinning roundtrip failed");
  Need(check.Meshes.SelectMany(m=>m.Vertices).All(v=>v.Colors[0].A==1f&&v.Colors[0].R==1f&&v.Colors[0].G==1f&&v.Colors[0].B==1f),"Native base vertex colour must remain full white");
  check.Meshes.Clear();check.Meshes.AddRange(oldMeshes);check.Header.BoundingBoxMin=oldMin;check.Header.BoundingBoxMax=oldMax;
  Need(check.Write().SequenceEqual(baseline),"Unrelated rig, shader, dummy or metadata changed");
  binder.Write(output);var after=BND4.Read(output);var before=BND4.Read(bytes);Need(after.Files.Count==before.Files.Count,"Binder count changed");
  for(int i=0;i<before.Files.Count;i++){var a=after.Files[i];var b=before.Files[i];Need(a.ID==b.ID&&a.Name==b.Name&&a.Flags==b.Flags&&a.CompressionType==b.CompressionType,"Binder identity changed");if(b.Name!="c0001.flv"&&!b.Name.StartsWith("DS2FX_Lantern_"))Need(a.Bytes.SequenceEqual(b.Bytes),"Unowned entry changed");}
  var audit=new{input_sha256=Hash(bytes),output_sha256=Hash(File.ReadAllBytes(output)),triangles,meshes=3,textures=9,original_restored_exactly=true,pivot_world=new[]{pivot.X,pivot.Y,pivot.Z},rig_and_wick_preserved=true,shader_preserved=true,packed_uv=true,baked_diffuse_and_normal=true,live_validated=false};
  File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine(JsonSerializer.Serialize(audit));
 }
}
