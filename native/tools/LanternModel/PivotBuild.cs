// SPDX-License-Identifier: GPL-3.0-or-later
// Offline, explicit shared-pivot candidate. This does not install or animate it.
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

static class PivotBuild {
    const string BoneName="DS2FX_LanternPivot";
    const int Pelvis=3;
    static readonly Vector3 PivotWorld=new(.27f,1.02f,-.055f);
    static readonly JsonSerializerOptions JsonOptions=new(){WriteIndented=true,IncludeFields=true};
    static void Check(bool ok,string why){if(!ok)throw new InvalidDataException(why);}
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
    static Matrix4x4 World(FLVER2 f,int i)=>f.Bones[i].ComputeLocalTransform()*(f.Bones[i].ParentIndex<0?Matrix4x4.Identity:World(f,f.Bones[i].ParentIndex));
    static float[] V(Vector3 v)=>new[]{v.X,v.Y,v.Z};
    sealed record Asset(string Input,string Output,BND4 Binder,BinderFile Entry,FLVER2 Model,byte[] Before,byte[] InputBytes);
    sealed record Added(int Index,int Tail,Vector3 Translation,Matrix4x4 World);
    static Asset Read(string input,string output,string expectedHash,string name,int boneCount){
        var bytes=File.ReadAllBytes(input);Check(Hash(bytes)==expectedHash,"Unsupported pivot source: "+input);
        var bnd=BND4.Read(bytes);var entry=bnd.Files.Single(x=>x.Name==name);var f=FLVER2.Read(entry.Bytes);
        Check(f.Bones.Count==boneCount&&f.Bones[Pelvis].Name=="Pelvis"&&f.Bones.All(x=>x.Name!=BoneName),"Unsupported source skeleton");
        return new(input,output,bnd,entry,f,f.Write(),bytes);
    }
    static Added Append(FLVER2 f){
        Check(Matrix4x4.Invert(World(f,Pelvis),out var inverse),"Singular pelvis transform");
        int tail=f.Bones[Pelvis].ChildIndex;Check(tail>=0,"Expected existing pelvis children");
        var seen=new HashSet<int>();int previous=-1;
        while(true){
            Check(tail>=0&&tail<f.Bones.Count&&seen.Add(tail),"Invalid pelvis sibling chain");
            var b=f.Bones[tail];Check(b.ParentIndex==Pelvis&&b.PreviousSiblingIndex==previous,"Inconsistent pelvis child links");
            if(b.NextSiblingIndex<0)break;previous=tail;tail=b.NextSiblingIndex;
        }
        int index=f.Bones.Count;
        var bone=new FLVER.Bone{Name=BoneName,ParentIndex=Pelvis,PreviousSiblingIndex=(short)tail,
            Translation=Vector3.Transform(PivotWorld,inverse),Rotation=Vector3.Zero,Scale=Vector3.One,
            BoundingBoxMin=new Vector3(float.MaxValue),BoundingBoxMax=new Vector3(float.MinValue),Unk3C=f.Bones[Pelvis].Unk3C};
        f.Bones[tail].NextSiblingIndex=(short)index;f.Bones.Add(bone);
        var world=World(f,index);Check(Vector3.Distance(Vector3.Transform(Vector3.Zero,world),PivotWorld)<1e-6f,"Pivot rest position changed");
        return new(index,tail,bone.Translation,world);
    }
    static void Remove(FLVER2 f,Added a){
        Check(f.Bones.Count==a.Index+1&&f.Bones[a.Index].Name==BoneName,"Pivot is not the sole appended bone");
        f.Bones.RemoveAt(a.Index);f.Bones[a.Tail].NextSiblingIndex=-1;
    }
    static object BoneAudit(FLVER2 f,Added a)=>new{
        name=BoneName,index=a.Index,bone_count=f.Bones.Count,parent="Pelvis",parent_index=Pelvis,
        local_translation_metres=V(a.Translation),local_rotation_euler_radians=V(Vector3.Zero),
        local_quaternion_xyzw=new[]{0f,0f,0f,1f},local_scale=V(Vector3.One),world_pivot_metres=V(PivotWorld),
        previous_last_child_index=a.Tail,previous_last_child_name=f.Bones[a.Tail].Name,
        original_hierarchy_metadata_change="Previous last Pelvis child NextSiblingIndex changes from -1 to the appended index. No existing parent, index, transform, or other link changes."
    };
    static void Write(Asset a){
        a.Binder.Write(a.Output);var read=BND4.Read(a.Output);var original=BND4.Read(a.InputBytes);
        Check(read.Files.Count==original.Files.Count,"Unexpected binder entries");
        for(int i=0;i<original.Files.Count;i++){
            var before=original.Files[i];var after=read.Files[i];
            Check(before.ID==after.ID&&before.Name==after.Name&&before.Flags==after.Flags&&before.CompressionType==after.CompressionType,"Binder identity/order changed");
            if(before.Name!=a.Entry.Name)Check(before.Bytes.SequenceEqual(after.Bytes),"Companion changed: "+before.Name);
        }
    }
    public static void Run(string baseInput,string bodyInput,string outputDirectory){
        var output=Path.GetFullPath(outputDirectory);
        Check(!Directory.Exists(output)&&!File.Exists(output),"Choose a fresh pivot output directory");
        var baseAsset=Read(baseInput,Path.Combine(output,"c0001.bnd"),"FA049EEDE38C6E989A46042C6DE9839B5FAB90CC7D8A76776BA7C2AFA5861798","c0001.flv",73);
        var bodyAsset=Read(bodyInput,Path.Combine(output,"bd_1030_m.bnd"),"43435A80165BD9AC3A21F5BBFC0AE3F19CB073E0E7FEB86DB0960EFC7500FB9E","BD_1030_M.flv",74);
        var baseF=baseAsset.Model;var bodyF=bodyAsset.Model;
        Check(baseF.Dummies.Count(x=>x.ReferenceID==247)==1,"Missing owned wick anchor");
        Check(baseF.Meshes.Count==0&&bodyF.Meshes.Count==13,"Unexpected candidate geometry");
        var dummy=baseF.Dummies.Single(x=>x.ReferenceID==247);
        Check(dummy.ParentBoneIndex==Pelvis&&dummy.AttachBoneIndex==-1,"Unexpected existing wick attachment");
        var oldDummyPosition=dummy.Position;var oldWickWorld=Vector3.Transform(dummy.Position,World(baseF,Pelvis));
        var baseBone=Append(baseF);var bodyBone=Append(bodyF);
        Check(baseBone.Translation==bodyBone.Translation&&baseBone.World==bodyBone.World,"Base/coat pivot rest transforms disagree");
        Check(Matrix4x4.Invert(baseBone.World,out var inversePivot),"Singular pivot transform");
        dummy.ParentBoneIndex=(short)baseBone.Index;dummy.Position=Vector3.Transform(oldWickWorld,inversePivot);
        var lampIndices=Enumerable.Range(0,bodyF.Meshes.Count).Where(i=>bodyF.Materials[bodyF.Meshes[i].MaterialIndex].Name.StartsWith("DS2FX_Lantern_",StringComparison.Ordinal)).ToArray();
        Check(lampIndices.SequenceEqual(new[]{9,10,11,12}),"Owned lamp meshes do not match source");
        float maxRestError=0;int vertexCount=0;
        foreach(int i in lampIndices){
            var mesh=bodyF.Meshes[i];Check(mesh.BoneIndices.SequenceEqual(new[]{Pelvis})&&mesh.DefaultBoneIndex==Pelvis,"Unexpected owned mesh palette");
            mesh.BoneIndices[0]=bodyBone.Index;mesh.DefaultBoneIndex=bodyBone.Index;
            foreach(var vertex in mesh.Vertices){
                Check(vertex.BoneIndices[0]==0&&vertex.BoneWeights[0]==1,"Lantern must be rigidly weighted");
                for(int k=1;k<4;k++)Check(vertex.BoneWeights[k]==0,"Additional lantern weights found");
                var local=Vector3.Transform(vertex.Position,inversePivot);
                var restored=Vector3.Transform(local,bodyBone.World);
                maxRestError=Math.Max(maxRestError,Vector3.Distance(restored,vertex.Position));vertexCount++;
                foreach(var f in new[]{baseF,bodyF}){
                    var b=f.Bones[f==baseF?baseBone.Index:bodyBone.Index];
                    b.BoundingBoxMin=Vector3.Min(b.BoundingBoxMin,local);b.BoundingBoxMax=Vector3.Max(b.BoundingBoxMax,local);
                }
            }
        }
        Check(maxRestError<2e-6f,"Rest skinning differs");
        baseAsset.Entry.Bytes=baseF.Write();bodyAsset.Entry.Bytes=bodyF.Write();
        var verifyBase=FLVER2.Read(baseAsset.Entry.Bytes);var verifyBody=FLVER2.Read(bodyAsset.Entry.Bytes);
        var addedDummy=verifyBase.Dummies.Single(x=>x.ReferenceID==247);
        float wickError=Vector3.Distance(Vector3.Transform(addedDummy.Position,World(verifyBase,baseBone.Index)),oldWickWorld);
        Check(wickError<1e-6f,"Wick rest position moved");
        Check(verifyBase.Bones[baseBone.Index].Translation==verifyBody.Bones[bodyBone.Index].Translation,"Rest translation roundtrip mismatch");
        // Restore only the declared edits; byte-equal serialization proves every other field stayed intact.
        addedDummy.ParentBoneIndex=Pelvis;addedDummy.Position=oldDummyPosition;Remove(verifyBase,baseBone);
        Check(verifyBase.Write().SequenceEqual(baseAsset.Before),"Undeclared original base-model change");
        foreach(int i in lampIndices){verifyBody.Meshes[i].BoneIndices[0]=Pelvis;verifyBody.Meshes[i].DefaultBoneIndex=Pelvis;}
        Remove(verifyBody,bodyBone);Check(verifyBody.Write().SequenceEqual(bodyAsset.Before),"Undeclared coat/vertex/material/rig change");
        Directory.CreateDirectory(output);Write(baseAsset);Write(bodyAsset);
        var bodyHash=Hash(File.ReadAllBytes(bodyAsset.Output));var baseHash=Hash(File.ReadAllBytes(baseAsset.Output));
        var baseAudit=new{
            role="shared-pivot-offline-candidate-not-live-verified",status="Content and rest-pose checks only; no runtime pose writes",source_sha256=Hash(baseAsset.InputBytes),output_sha256=baseHash,
            dummy=247,parent_bone=BoneName,parent_bone_index=baseBone.Index,attach_bone=-1,stored_bone_position=V(dummy.Position),
            wick_world_metres=V(oldWickWorld),wick_rest_position_error_metres=wickError,pivot=BoneAudit(baseF,baseBone),
            original_dummies_preserved=baseF.Dummies.Count,original_flver_restored_exactly=true,original_companions_preserved=baseAsset.Binder.Files.Count-1,
            matching_model_sha256=bodyHash,physics="No motion until a validated post-animation native setter writes the owned bone; absent animation mapping resets it to bind pose."
        };
        var bodyAudit=new{
            role="shared-pivot-offline-candidate-not-live-verified",input_sha256=Hash(bodyAsset.InputBytes),output_sha256=bodyHash,
            source_armor="Wanderer Coat male",armor_item_id=21030101,pivot=BoneAudit(bodyF,bodyBone),
            owned_mesh_indices=lampIndices,owned_vertices=vertexCount,owned_mesh_bone_palette=new[]{bodyBone.Index},
            existing_vertex_fields_unchanged=true,original_meshes_preserved=9,all_original_bone_transforms_indices_preserved=true,
            original_flver_restored_exactly=true,original_companions_preserved=bodyAsset.Binder.Files.Count-1,
            maximum_rest_skinning_roundtrip_error_metres=maxRestError,offset_metres=V(PivotWorld),open_windows=true,
            hkx_and_clmp_byte_identical=true,scope="Only added shared bone and four owned mesh palettes. No new animation tracks or cloth edits; no runtime motion claim."
        };
        File.WriteAllText(baseAsset.Output+".audit.json",JsonSerializer.Serialize(baseAudit,JsonOptions));
        File.WriteAllText(bodyAsset.Output+".audit.json",JsonSerializer.Serialize(bodyAudit,JsonOptions));
        var report=new{Base=baseAudit,Body=bodyAudit,Deployed=false};
        File.WriteAllText(Path.Combine(output,"pivot.audit.json"),JsonSerializer.Serialize(report,JsonOptions));
        Console.WriteLine(JsonSerializer.Serialize(report,JsonOptions));
    }
}
