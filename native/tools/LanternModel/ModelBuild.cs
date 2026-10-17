// SPDX-License-Identifier: GPL-3.0-or-later
// Author-owned geometry/texture recipe. Requires the user's exact local game asset.
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

static class ModelBuild {
    static void Check(bool ok,string why) {if(!ok)throw new InvalidDataException(why);}
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
    static Vector3 V(JsonElement a)=>new(a[0].GetSingle(),a[1].GetSingle(),a[2].GetSingle());
    static Matrix4x4 BoneWorld(FLVER2 f,int i) {
        var b=f.Bones[i];return b.ComputeLocalTransform()*(b.ParentIndex<0?Matrix4x4.Identity:BoneWorld(f,b.ParentIndex));
    }
    public static void Run(string donor,string geometry,string output) {
        Check(!File.Exists(output),"Output exists; use a new path");
        var bytes=File.ReadAllBytes(donor);
        Check(Hash(bytes)=="5427F97D3EC36408B1B8EC9CEF09E1E615C369541F295389675748E5845F3B73","Unsupported Wanderer Coat source");
        var bnd=BND4.Read(bytes);var original=bnd.Files.Select(f=>(f.ID,f.Name,f.Flags,Bytes:f.Bytes.ToArray())).ToArray();
        var file=bnd.Files.Single(f=>f.Name=="BD_1030_M.flv");var f=FLVER2.Read(file.Bytes);
        Check(f.Bones[3].Name=="Pelvis" && f.Meshes.Count==9 && f.Materials.Count==9,"Unexpected donor structure");
        var donorMesh=f.Meshes[0];Check(donorMesh.Dynamic==1 && donorMesh.VertexBuffers.Count==1 && donorMesh.VertexBuffers[0].LayoutIndex==1,"Unexpected skinned vertex layout");
        var prototype=new FLVER.Vertex(donorMesh.Vertices[0]);
        // Rigidly skin to the existing pelvis. Keep original rig and cloth intact.
        Matrix4x4.Invert(BoneWorld(f,3),out var inversePelvis);
        using var doc=JsonDocument.Parse(File.ReadAllText(geometry));
        bool openWindows=doc.RootElement.TryGetProperty("open_windows",out var windows)&&windows.GetBoolean();
        var offset=new Vector3(.27f,1.02f,-.055f);
        var meshCount=f.Meshes.Count;int triangles=0,addedMaterials=0;
        var palette=new[]{new Vector3(104,66,27),new Vector3(32,38,40),new Vector3(99,39,8),new Vector3(53,26,13),new Vector3(88,62,34)};
        var baseTpf=TPF.Read(bnd.Files.First(x=>x.Name.EndsWith("_d.tpf")).Bytes);
        int nextId=bnd.Files.Max(x=>x.ID)+1;
        foreach(var group in doc.RootElement.GetProperty("groups").EnumerateArray()) {
            var matIndex=group.GetProperty("material").GetInt32();Check(matIndex>=0&&matIndex<palette.Length,"Unknown material");
            if(group.GetProperty("triangles").GetArrayLength()==0) {
                Check(openWindows&&matIndex==2,"Only the declared open-window pane group may be empty");continue;
            }
            if(openWindows)Check(matIndex!=2,"Open-window geometry still contains opaque panes");
            addedMaterials++;
            string texBase="DS2FX_Lantern_"+matIndex;
            foreach(string suffix in new[]{"d","n","s"}) {
                var tpf=new TPF{Platform=baseTpf.Platform,Encoding=baseTpf.Encoding,Flag2=baseTpf.Flag2};
                var pixels=Texture(palette[matIndex],suffix,matIndex);
                tpf.Textures.Add(new TPF.Texture(texBase+"_"+suffix,9,0,pixels));
                var payload=tpf.Write();Check(TPF.Read(payload).Textures.Single().Name==texBase+"_"+suffix,"TPF roundtrip failed");
                bnd.Files.Add(new BinderFile(file.Flags,nextId++,texBase+"_"+suffix+".tpf",payload));
            }
            var source=f.Materials[0];
            var material=new FLVER2.Material("DS2FX_Lantern_"+matIndex,source.MTD,source.Flags){GXIndex=source.GXIndex,Unk18=source.Unk18};
            foreach(var t in source.Textures) {
                var suffix=t.Type=="g_DiffuseTexture"?"d":t.Type=="g_SpecularTexture"?"s":t.Type=="g_BumpmapTexture"?"n":null;
                material.Textures.Add(new FLVER2.Texture(t.Type,suffix==null?t.Path:"N:\\FRPG2\\data\\model\\parts\\DS2FX\\"+texBase+"_"+suffix+".tga",t.Scale,t.Unk10,t.Unk11,t.Unk14,t.Unk18,t.Unk1C));
            }
            var mesh=new FLVER2.Mesh{Dynamic=1,MaterialIndex=f.Materials.Count,DefaultBoneIndex=3};
            mesh.BoneIndices.Add(3);mesh.VertexBuffers.Add(new FLVER2.VertexBuffer(1));
            var indices=new List<int>();
            foreach(var tri in group.GetProperty("triangles").EnumerateArray()) {
                Check(tri.GetArrayLength()==3,"Expected triangles");triangles++;
                foreach(var vertex in tri.EnumerateArray()) {
                    var v=new FLVER.Vertex(prototype);v.Position=V(vertex.GetProperty("p"))+offset;v.Normal=Vector3.Normalize(V(vertex.GetProperty("n")));
                    Check(float.IsFinite(v.Position.X)&&float.IsFinite(v.Position.Y)&&float.IsFinite(v.Position.Z),"Non-finite vertex");
                    v.BoneIndices[0]=0;v.BoneWeights[0]=1;
                    for(int k=1;k<4;k++){v.BoneIndices[k]=0;v.BoneWeights[k]=0;}
                    v.NormalW=0;var uv=vertex.GetProperty("uv");v.UVs[0]=new Vector3(uv[0].GetSingle(),uv[1].GetSingle(),0);
                    var tangent=Vector3.Normalize(Vector3.Cross(Math.Abs(v.Normal.Y)<.9?Vector3.UnitY:Vector3.UnitX,v.Normal));
                    v.Tangents[0]=new Vector4(tangent,-1);
                    v.Colors[0]=new FLVER.VertexColor(1f,1f,1f,1f);
                    indices.Add(mesh.Vertices.Count);mesh.Vertices.Add(v);
                    var local=Vector3.Transform(v.Position,inversePelvis);
                    f.Bones[3].BoundingBoxMin=Vector3.Min(f.Bones[3].BoundingBoxMin,local);
                    f.Bones[3].BoundingBoxMax=Vector3.Max(f.Bones[3].BoundingBoxMax,local);
                    f.Header.BoundingBoxMin=Vector3.Min(f.Header.BoundingBoxMin,v.Position);
                    f.Header.BoundingBoxMax=Vector3.Max(f.Header.BoundingBoxMax,v.Position);
                }
            }
            Check(mesh.Vertices.Count>0&&mesh.Vertices.Count<65535,"Invalid mesh size");
            mesh.FaceSets.Add(new FLVER2.FaceSet(FLVER2.FaceSet.FSFlags.None,false,true,0,indices));
            mesh.FaceSets.Add(new FLVER2.FaceSet(FLVER2.FaceSet.FSFlags.MotionBlur,false,true,0,new List<int>(indices)));
            f.Materials.Add(material);f.Meshes.Add(mesh);
        }
        file.Bytes=f.Write();var reread=FLVER2.Read(file.Bytes);
        Check(reread.Meshes.Count==meshCount+addedMaterials && addedMaterials==(openWindows?4:5) && reread.Bones.Count==74,"Model structure did not survive roundtrip");
        foreach(var mesh in reread.Meshes.Skip(meshCount)) {
            Check(mesh.BoneIndices.SequenceEqual(new[]{3}),"Wrong pelvis palette");
            foreach(var v in mesh.Vertices) Check(v.BoneIndices[0]==0&&v.BoneWeights[0]==1,"Rigid pelvis weight lost");
        }
        var donorFlver=FLVER2.Read(original.Single(x=>x.Name==file.Name).Bytes);
        for(int i=0;i<meshCount;i++) {
            var before=donorFlver.Meshes[i];var after=reread.Meshes[i];
            Check(before.MaterialIndex==after.MaterialIndex&&before.DefaultBoneIndex==after.DefaultBoneIndex&&before.Vertices.Count==after.Vertices.Count,"Original mesh altered");
            for(int k=0;k<before.Vertices.Count;k++) Check(before.Vertices[k].Position==after.Vertices[k].Position,"Original vertex moved");
            for(int k=0;k<before.FaceSets.Count;k++)Check(before.FaceSets[k].Indices.SequenceEqual(after.FaceSets[k].Indices),"Original face changed");
        }
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);bnd.Write(output);
        var result=BND4.Read(output);
        foreach(var old in original) {
            var now=result.Files.Single(x=>x.ID==old.ID);Check(now.Name==old.Name&&now.Flags==old.Flags,"Original binder identity changed");
            if(old.Name!=file.Name)Check(now.Bytes.SequenceEqual(old.Bytes),"Original companion asset changed");
        }
        var audit=new{status="Owned rigid hip-model fixture; not live validated",input_sha256=Hash(bytes),geometry_sha256=Hash(File.ReadAllBytes(geometry)),output_sha256=Hash(File.ReadAllBytes(output)),
            source_armor="Wanderer Coat male",armor_item_id=21030101,bone="Pelvis",bone_index=3,
            offset_metres=new[]{offset.X,offset.Y,offset.Z},added_meshes=reread.Meshes.Count-meshCount,added_triangles=triangles,original_meshes_preserved=meshCount,original_companions_preserved=original.Length-1,
            original_rig_bones=74,added_textures=addedMaterials*3,texture_format="64x64 BGRA8 DDS inside TPF; original procedural recipes",open_windows=openWindows,material=openWindows?"Existing body shader; pane geometry omitted, open cage windows":"Existing body shader; amber panes opaque in this first fixture",scope="Only male Wanderer Coat. No gameplay stats/cloth/animation/other armor edits. No physical sway yet."};
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine(JsonSerializer.Serialize(audit));
    }
    static byte[] Texture(Vector3 color,string suffix,int material) {
        const int side=64;var pixels=new byte[side*side*4];
        for(int y=0;y<side;y++)for(int x=0;x<side;x++) {
            uint h=unchecked((uint)(x*374761393+y*668265263+material*31));h=(h^(h>>13))*1274126177u;
            float grain=.76f+(h%1000)/4000f;
            var c=suffix=="n"?new Vector3(128,128,255):suffix=="s"?new Vector3(material is 0 or 1?130:25):color*grain;
            if(suffix=="d" && (x+3*y)%53==0)c*=.6f;
            int at=(y*side+x)*4;pixels[at]=(byte)c.Z;pixels[at+1]=(byte)c.Y;pixels[at+2]=(byte)c.X;pixels[at+3]=255;
        }
        var dds=new DDS{dwWidth=side,dwHeight=side,dwPitchOrLinearSize=side*4,dwMipMapCount=1};
        dds.dwFlags|=DDS.DDSD.PITCH;
        dds.ddspf.dwFlags=DDS.DDPF.RGB|DDS.DDPF.ALPHAPIXELS;dds.ddspf.dwRGBBitCount=32;
        dds.ddspf.dwRBitMask=0x00ff0000;dds.ddspf.dwGBitMask=0x0000ff00;dds.ddspf.dwBBitMask=0x000000ff;dds.ddspf.dwABitMask=0xff000000u;
        return dds.Write(pixels);
    }
}
