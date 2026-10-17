// SPDX-License-Identifier: GPL-3.0-or-later
// Replaces only the owned base-character lantern geometry and its twelve TPFs.
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

static class BaseRefresh {
    const string ExpectedInput="32752972E4C073D038C44AB2CEDBBA0894DB22365509E2E07823404E4FEFCE52";
    const int Pivot=73;
    static readonly Vector3 PivotWorld=new(.27f,1.02f,-.055f);
    static void Check(bool ok,string why){if(!ok)throw new InvalidDataException(why);}
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
    static Vector3 V(JsonElement a)=>new(a[0].GetSingle(),a[1].GetSingle(),a[2].GetSingle());
    static byte Byte(float value)=>(byte)Math.Clamp((int)MathF.Round(value*255f),0,255);
    sealed record Profile(string Name, Vector3 BaseColor, float Metallic, float Roughness);

    public static void Run(string input,string geometry,string output) {
        Check(!File.Exists(output),"Output exists; choose a fresh candidate path");
        var inputBytes=File.ReadAllBytes(input); Check(Hash(inputBytes)==ExpectedInput,"Unsupported v11 base source");
        var binder=BND4.Read(inputBytes); var file=binder.Files.Single(x=>x.Name=="c0001.flv"); var f=FLVER2.Read(file.Bytes);
        Check(f.Bones.Count==74&&f.Bones[Pivot].Name=="DS2FX_LanternPivot"&&f.Bones[Pivot].ParentIndex==3,"Unexpected v11 pivot skeleton");
        Check(f.Meshes.Count==4&&f.Materials.Count==4&&f.Meshes.All(m=>m.DefaultBoneIndex==Pivot&&m.BoneIndices.SequenceEqual(new[]{Pivot})),"Input does not contain the owned base lantern only");
        Check(f.Materials.All(m=>m.Name.StartsWith("DS2FX_Lantern_",StringComparison.Ordinal)),"Unexpected base material ownership");
        var prototype=new FLVER.Vertex(f.Meshes[0].Vertices[0]); var sourceMaterial=f.Materials[0]; var sourceLayout=f.BufferLayouts[f.Meshes[0].VertexBuffers[0].LayoutIndex]; var sourceGx=sourceMaterial.GXIndex>=0?f.GXLists[sourceMaterial.GXIndex]:null; var baseTpf=TPF.Read(binder.Files.First(x=>x.Name.EndsWith("_d.tpf",StringComparison.Ordinal)).Bytes);
        var originalEntries=binder.Files.Select(x=>(x.ID,x.Name,x.Flags,x.CompressionType,Bytes:x.Bytes.ToArray())).ToArray();
        using var doc=JsonDocument.Parse(File.ReadAllText(geometry));
        Check(doc.RootElement.GetProperty("open_windows").GetBoolean(),"The v13 lantern must retain open cage windows");
        var profiles=new Dictionary<int,Profile>(); int idx=0;
        foreach(var p in doc.RootElement.GetProperty("materials").EnumerateArray()) {
            var n=p.GetProperty("name").GetString()??""; var color=V(p.GetProperty("base_color"));
            profiles[idx++]=new Profile(n,color,p.GetProperty("metallic").GetSingle(),p.GetProperty("roughness").GetSingle());
        }
        Check(profiles.Count==5&&profiles[0].Name=="01 Weathered brass"&&profiles[1].Name=="02 Blackened iron"&&profiles[4].Name=="05 Wick ceramic","Unexpected Blender material map");
        var retained=binder.Files.Where(x=>!x.Name.StartsWith("DS2FX_Lantern_",StringComparison.Ordinal)).ToList(); binder.Files.Clear(); binder.Files.AddRange(retained);
        f.Meshes.Clear(); f.Materials.Clear(); f.BufferLayouts.Clear(); f.GXLists.Clear(); f.BufferLayouts.Add(sourceLayout); if(sourceGx!=null)f.GXLists.Add(sourceGx);
        int nextId=binder.Files.Max(x=>x.ID)+1, added=0, triangles=0;
        foreach(var group in doc.RootElement.GetProperty("groups").EnumerateArray()) {
            int material=group.GetProperty("material").GetInt32(); var faces=group.GetProperty("triangles"); if(faces.GetArrayLength()==0) continue;
            Check(material is 0 or 1 or 4,"Only the saved brass, iron, and wick materials may contain geometry");
            var profile=profiles[material]; string texBase="DS2FX_Lantern_"+material;
            foreach(var suffix in new[]{"d","n","s"}) {
                var tpf=new TPF{Platform=baseTpf.Platform,Encoding=baseTpf.Encoding,Flag2=baseTpf.Flag2};
                tpf.Textures.Add(new TPF.Texture(texBase+"_"+suffix,9,0,Texture(profile,suffix,material)));
                var payload=tpf.Write(); Check(TPF.Read(payload).Textures.Single().Name==texBase+"_"+suffix,"TPF roundtrip failed");
                binder.Files.Add(new BinderFile(file.Flags,nextId++,texBase+"_"+suffix+".tpf",payload));
            }
            var mat=new FLVER2.Material(texBase,sourceMaterial.MTD,sourceMaterial.Flags){GXIndex=sourceGx==null?-1:0,Unk18=sourceMaterial.Unk18};
            foreach(var t in sourceMaterial.Textures) {
                var suffix=t.Type=="g_DiffuseTexture"?"d":t.Type=="g_SpecularTexture"?"s":t.Type=="g_BumpmapTexture"?"n":null;
                mat.Textures.Add(new FLVER2.Texture(t.Type,suffix==null?t.Path:"N:\\FRPG2\\data\\model\\parts\\DS2FX\\"+texBase+"_"+suffix+".tga",t.Scale,t.Unk10,t.Unk11,t.Unk14,t.Unk18,t.Unk1C));
            }
            var mesh=new FLVER2.Mesh{Dynamic=1,MaterialIndex=f.Materials.Count,DefaultBoneIndex=Pivot}; mesh.BoneIndices.Add(Pivot); mesh.VertexBuffers.Add(new FLVER2.VertexBuffer(0)); var indices=new List<int>();
            foreach(var tri in faces.EnumerateArray()) { Check(tri.GetArrayLength()==3,"Expected triangles"); triangles++;
                foreach(var vertex in tri.EnumerateArray()) { var v=new FLVER.Vertex(prototype); v.Position=V(vertex.GetProperty("p"))+PivotWorld; v.Normal=Vector3.Normalize(V(vertex.GetProperty("n")));
                    v.BoneIndices[0]=0; v.BoneWeights[0]=1; for(int k=1;k<4;k++){v.BoneIndices[k]=0;v.BoneWeights[k]=0;} v.NormalW=0; var uv=vertex.GetProperty("uv"); v.UVs[0]=new Vector3(uv[0].GetSingle(),uv[1].GetSingle(),0);
                    var tangent=Vector3.Normalize(Vector3.Cross(Math.Abs(v.Normal.Y)<.9f?Vector3.UnitY:Vector3.UnitX,v.Normal)); v.Tangents[0]=new Vector4(tangent,-1); v.Colors[0]=new FLVER.VertexColor(1f,1f,1f,1f); indices.Add(mesh.Vertices.Count);mesh.Vertices.Add(v);
                    f.Header.BoundingBoxMin=Vector3.Min(f.Header.BoundingBoxMin,v.Position);f.Header.BoundingBoxMax=Vector3.Max(f.Header.BoundingBoxMax,v.Position);
                }
            }
            mesh.FaceSets.Add(new FLVER2.FaceSet(FLVER2.FaceSet.FSFlags.None,false,true,0,indices)); mesh.FaceSets.Add(new FLVER2.FaceSet(FLVER2.FaceSet.FSFlags.MotionBlur,false,true,0,new List<int>(indices))); f.Materials.Add(mat);f.Meshes.Add(mesh);added++;
        }
        Check(added==3&&f.Meshes.Count==3&&f.Materials.Count==3&&triangles>0,"Unexpected v13 owned mesh count"); file.Bytes=f.Write(); var reread=FLVER2.Read(file.Bytes);
        Check(reread.Bones.Count==74&&reread.Bones[Pivot].Name=="DS2FX_LanternPivot"&&reread.Dummies.Count==f.Dummies.Count,"Pivot or dummy changed");
        Check(reread.Meshes.All(m=>m.DefaultBoneIndex==Pivot&&m.BoneIndices.SequenceEqual(new[]{Pivot})&&m.Vertices.All(v=>v.BoneIndices[0]==0&&v.BoneWeights[0]==1)),"New lantern skinning invalid");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!); binder.Write(output); var check=BND4.Read(output);
        foreach(var old in originalEntries.Where(x=>x.Name!="c0001.flv"&&!x.Name.StartsWith("DS2FX_Lantern_",StringComparison.Ordinal))) { var now=check.Files.Single(x=>x.ID==old.ID); Check(now.Name==old.Name&&now.Flags==old.Flags&&now.CompressionType==old.CompressionType&&now.Bytes.SequenceEqual(old.Bytes),"Unowned binder entry changed: "+old.Name); }
        var audit=new{role="manual-blender-v13-base-refresh",input_sha256=Hash(inputBytes),geometry_sha256=Hash(File.ReadAllBytes(geometry)),output_sha256=Hash(File.ReadAllBytes(output)),blend_materials=profiles.Select(x=>new{slot=x.Key,name=x.Value.Name,base_color=new[]{x.Value.BaseColor.X,x.Value.BaseColor.Y,x.Value.BaseColor.Z},x.Value.Metallic,x.Value.Roughness}),owned_meshes=added,owned_triangles=triangles,owned_textures=added*3,pivot="DS2FX_LanternPivot",pivot_index=Pivot,open_windows=true,unowned_binder_entries_preserved=true,status="Static candidate only; requires Root Builder deployment and in-game visual validation"};
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true})); Console.WriteLine(JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));
    }
    static byte[] Texture(Profile p,string suffix,int material) {
        const int side=64; var pixels=new byte[side*side*4];
        for(int y=0;y<side;y++)for(int x=0;x<side;x++){ uint h=unchecked((uint)(x*374761393+y*668265263+material*31));h=(h^(h>>13))*1274126177u; float grain=.76f+(h%1000)/4000f;
            float specular=18f+(170f-18f)*(p.Metallic*(1f-p.Roughness*.45f)); Vector3 c=suffix=="n"?new Vector3(128f,128f,255f):suffix=="s"?new Vector3(specular):p.BaseColor*255f*grain; if(suffix=="d"&&(x+3*y)%53==0)c*=.6f; int at=(y*side+x)*4;pixels[at]=Byte(c.Z/255f);pixels[at+1]=Byte(c.Y/255f);pixels[at+2]=Byte(c.X/255f);pixels[at+3]=255; }
        var dds=new DDS{dwWidth=side,dwHeight=side,dwPitchOrLinearSize=side*4,dwMipMapCount=1};dds.dwFlags|=DDS.DDSD.PITCH;dds.ddspf.dwFlags=DDS.DDPF.RGB|DDS.DDPF.ALPHAPIXELS;dds.ddspf.dwRGBBitCount=32;dds.ddspf.dwRBitMask=0x00ff0000;dds.ddspf.dwGBitMask=0x0000ff00;dds.ddspf.dwBBitMask=0x000000ff;dds.ddspf.dwABitMask=0xff000000u;return dds.Write(pixels);
    }
}
