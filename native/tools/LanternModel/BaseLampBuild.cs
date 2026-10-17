// SPDX-License-Identifier: GPL-3.0-or-later
// Moves only owned lamp geometry into the character model. Never edits armor.
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

static class BaseLampBuild {
    static void Check(bool ok,string reason){if(!ok)throw new InvalidDataException(reason);}
    static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes));
    public static void Run(string baseInput,string lampInput,string output){
        Check(!File.Exists(output),"Choose a fresh output path");
        byte[] baseBytes=File.ReadAllBytes(baseInput),lampBytes=File.ReadAllBytes(lampInput);
        Check(Hash(baseBytes)=="D8851F86E33673DC92408BF25AD55EAEF085EF09A79C08DB142B33BB48FF08EA","Unsupported pivot base source");
        Check(Hash(lampBytes)=="0C23F85CBF3E742D53DFEB05BB5E4A627565785DFFD436985CB42062ECE5DF3B","Unsupported pivot lamp source");
        var binder=BND4.Read(baseBytes);var source=BND4.Read(lampBytes);
        var file=binder.Files.Single(x=>x.Name=="c0001.flv");var f=FLVER2.Read(file.Bytes);
        var lamp=FLVER2.Read(source.Files.Single(x=>x.Name=="BD_1030_M.flv").Bytes);
        Check(f.Meshes.Count==0&&f.Materials.Count==0&&f.BufferLayouts.Count==0&&f.GXLists.Count==0,"Base is not the supported empty drawable model");
        Check(f.Bones.Count==74&&f.Bones[73].Name=="DS2FX_LanternPivot"&&lamp.Bones[74].Name==f.Bones[73].Name,"Shared pivot mismatch");
        Check(f.Bones[73].Translation==lamp.Bones[74].Translation&&f.Bones[73].Rotation==lamp.Bones[74].Rotation&&f.Bones[73].Scale==lamp.Bones[74].Scale,"Pivot local bind mismatch");
        var before=f.Write();var oldMin=f.Header.BoundingBoxMin;var oldMax=f.Header.BoundingBoxMax;
        var layouts=new Dictionary<int,int>();var gx=new Dictionary<int,int>();int vertices=0;
        for(int i=9;i<13;i++){
            var mesh=lamp.Meshes[i];var material=lamp.Materials[mesh.MaterialIndex];
            Check(material.Name.StartsWith("DS2FX_Lantern_",StringComparison.Ordinal)&&mesh.BoneIndices.SequenceEqual(new[]{74}),"Unexpected lamp identity/palette");
            if(material.GXIndex>=0){
                if(!gx.TryGetValue(material.GXIndex,out int to)){to=f.GXLists.Count;gx.Add(material.GXIndex,to);f.GXLists.Add(lamp.GXLists[material.GXIndex]);}
                material.GXIndex=to;
            }
            mesh.MaterialIndex=f.Materials.Count;f.Materials.Add(material);
            foreach(var vb in mesh.VertexBuffers){
                if(!layouts.TryGetValue(vb.LayoutIndex,out int to)){to=f.BufferLayouts.Count;layouts.Add(vb.LayoutIndex,to);f.BufferLayouts.Add(lamp.BufferLayouts[vb.LayoutIndex]);}
                vb.LayoutIndex=to;
            }
            mesh.DefaultBoneIndex=73;mesh.BoneIndices[0]=73;f.Meshes.Add(mesh);vertices+=mesh.Vertices.Count;
            foreach(var v in mesh.Vertices){f.Header.BoundingBoxMin=Vector3.Min(f.Header.BoundingBoxMin,v.Position);f.Header.BoundingBoxMax=Vector3.Max(f.Header.BoundingBoxMax,v.Position);}
        }
        var textures=source.Files.Where(x=>x.Name.StartsWith("DS2FX_Lantern_",StringComparison.Ordinal)&&x.Name.EndsWith(".tpf",StringComparison.Ordinal)).ToArray();
        Check(textures.Length==12,"Expected twelve owned textures");int nextId=binder.Files.Max(x=>x.ID)+1;
        foreach(var texture in textures){Check(!binder.Files.Any(x=>x.Name==texture.Name),"Texture collision");binder.Files.Add(new BinderFile(texture.Flags,nextId++,texture.Name,texture.Bytes){CompressionType=texture.CompressionType});}
        file.Bytes=f.Write();var read=FLVER2.Read(file.Bytes);
        Check(read.Meshes.Count==4&&read.Materials.Count==4&&read.Bones.Count==74,"Base lamp structure failed roundtrip");
        for(int i=0;i<4;i++){
            var actual=read.Meshes[i];var expected=lamp.Meshes[9+i];
            Check(actual.BoneIndices.SequenceEqual(new[]{73})&&actual.DefaultBoneIndex==73,"Base lamp weight palette mismatch");
            Check(actual.Vertices.Count==expected.Vertices.Count,"Vertex count changed");
            for(int v=0;v<actual.Vertices.Count;v++)Check(actual.Vertices[v].Position==expected.Vertices[v].Position,"Owned vertex position changed");
        }
        // Removing only added draw data and restoring header bounds must recover every prior field.
        read.Meshes.Clear();read.Materials.Clear();read.BufferLayouts.Clear();read.GXLists.Clear();read.Header.BoundingBoxMin=oldMin;read.Header.BoundingBoxMax=oldMax;
        Check(read.Write().SequenceEqual(before),"Undeclared base skeleton/dummy/metadata change");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);binder.Write(output);
        var check=BND4.Read(output);var original=BND4.Read(baseBytes);
        for(int i=0;i<original.Files.Count;i++){
            var a=original.Files[i];var b=check.Files[i];Check(a.ID==b.ID&&a.Name==b.Name&&a.Flags==b.Flags&&a.CompressionType==b.CompressionType,"Original binder identity changed");
            if(a.Name!=file.Name)Check(a.Bytes.SequenceEqual(b.Bytes),"Original base companion changed");
        }
        foreach(var texture in textures)Check(check.Files.Single(x=>x.Name==texture.Name).Bytes.SequenceEqual(texture.Bytes),"Owned texture bytes changed");
        using var prior=JsonDocument.Parse(File.ReadAllText(baseInput+".audit.json"));
        var audit=new{
            role="base-model-lantern-offline-candidate-not-live-verified",source_sha256=Hash(baseBytes),lamp_source_sha256=Hash(lampBytes),output_sha256=Hash(File.ReadAllBytes(output)),
            dummy=247,parent_bone="DS2FX_LanternPivot",parent_bone_index=73,pivot=prior.RootElement.GetProperty("pivot").Clone(),
            stored_bone_position=prior.RootElement.GetProperty("stored_bone_position").Clone(),wick_world_metres=prior.RootElement.GetProperty("wick_world_metres").Clone(),
            owned_meshes=4,owned_vertices=vertices,owned_textures=12,original_flver_restored_exactly=true,original_companions_preserved=5,
            armor_override_required=false,bone_count=74,existing_bone_dummy_fields_unchanged=true,all_armor_gender_coverage_live_verified=false,
            base_draw_evidence="CharacterModelCtrl31A6B0 calls inline base model virtual+10; vtable10DB650 selects2F08E0, which reaches2F2CB0 draw. Visibility/resource predicates still need in-game verification.",
            scope="Lamp lives on base character; no armor binder is packaged. Existing body-specific lamp override must be removed by the deployment owner to avoid duplicates.",Deployed=false
        };
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine(JsonSerializer.Serialize(audit));
    }
}
