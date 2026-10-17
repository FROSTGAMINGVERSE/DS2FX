// SPDX-License-Identifier: GPL-3.0-or-later
// Adds one owned particle anchor; never rewrites an existing game dummy.
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

static class FlameAnchor {
    static void Check(bool ok,string reason){if(!ok)throw new InvalidDataException(reason);}
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b));
    static Matrix4x4 World(FLVER2 f,int i)=>f.Bones[i].ComputeLocalTransform()*(f.Bones[i].ParentIndex<0?Matrix4x4.Identity:World(f,f.Bones[i].ParentIndex));
    public static void Build(string input,string output,string? model=null) {
        Check(!File.Exists(output),"Output exists; choose a fresh path");
        var bytes=File.ReadAllBytes(input);
        string modelHash="F8C91BB8C5C38C4B471FCD25BB258BADC5BBB23D507B47B5B690C7013FAEDF22";
        if(model!=null) {
            using var modelAudit=JsonDocument.Parse(File.ReadAllText(model+".audit.json"));var ma=modelAudit.RootElement;
            modelHash=Hash(File.ReadAllBytes(model));
            Check(ma.GetProperty("output_sha256").GetString()==modelHash&&ma.GetProperty("input_sha256").GetString()=="5427F97D3EC36408B1B8EC9CEF09E1E615C369541F295389675748E5845F3B73","Invalid owned-model audit");
            var offset=ma.GetProperty("offset_metres");Check(offset[0].GetSingle()==.27f&&offset[1].GetSingle()==1.02f&&offset[2].GetSingle()==-.055f,"Model pivot differs from wick anchor contract");
        }
        Check(Hash(bytes)=="A30763D0F1BA1C19D4ABC6AD9634FA40463E2D871638C2079F23B4F4BD080ED5","Unsupported c0001 source");
        var binder=BND4.Read(bytes);var original=binder.Files.Select(x=>(x.ID,x.Name,x.Flags,Bytes:x.Bytes.ToArray())).ToArray();
        var entry=binder.Files.Single(x=>x.Name=="c0001.flv");var f=FLVER2.Read(entry.Bytes);
        Check(f.Bones[3].Name=="Pelvis"&&f.Dummies.All(d=>d.ReferenceID!=247),"Pelvis or unused dummy247 mismatch");
        byte[] before=f.Write();int count=f.Dummies.Count;
        var source=f.Dummies.Single(x=>x.ReferenceID==30);
        Check(source.ParentBoneIndex==3&&source.AttachBoneIndex==-1,"Unexpected reference pelvis dummy");
        // The owned model's belt pivot is(.27,1.02,-.055); wick top is0.198m below.
        var wickWorld=new Vector3(.27f,.822f,-.055f);
        Check(Matrix4x4.Invert(World(f,3),out var inverse),"Singular pelvis bind transform");
        var point=Vector3.Transform(wickWorld,inverse);
        f.Dummies.Add(new FLVER.Dummy{ReferenceID=247,ParentBoneIndex=3,AttachBoneIndex=-1,Position=point,
            Forward=source.Forward,Upward=source.Upward,Color=source.Color,Flag1=source.Flag1,UseUpwardVector=source.UseUpwardVector,Unk30=source.Unk30,Unk34=source.Unk34});
        entry.Bytes=f.Write();var reread=FLVER2.Read(entry.Bytes);
        Check(reread.Dummies.Count==count+1,"Dummy count failed roundtrip");
        var added=reread.Dummies.Single(d=>d.ReferenceID==247);
        Check(Vector3.Distance(Vector3.Transform(added.Position,World(reread,3)),wickWorld)<0.00001f,"Wick anchor mismatch");
        reread.Dummies.Remove(added);
        Check(reread.Write().SequenceEqual(before),"Existing model/rig/dummy data changed");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);binder.Write(output);
        var verify=BND4.Read(output);
        foreach(var old in original){var now=verify.Files.Single(x=>x.ID==old.ID);Check(now.Name==old.Name&&now.Flags==old.Flags,"Binder identity changed");if(old.Name!=entry.Name)Check(now.Bytes.SequenceEqual(old.Bytes),"Companion changed");}
        var audit=new{status="Static wick-anchor fixture; live attachment/orientation/scale unverified",source_sha256=Hash(bytes),output_sha256=Hash(File.ReadAllBytes(output)),dummy=247,parent_bone="Pelvis",parent_bone_index=3,attach_bone=-1,
            wick_world_metres=new[]{wickWorld.X,wickWorld.Y,wickWorld.Z},stored_bone_position=new[]{point.X,point.Y,point.Z},original_dummies_preserved=count,original_flver_restored_exactly=true,original_companions_preserved=original.Length-1,
            matching_model_sha256=modelHash,physics="Existing pelvis animation only; no pendulum physics",scope="New unused base-character particle anchor; normal dummies unchanged. Armor eligibility remains runtime work."};
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(audit,new JsonSerializerOptions{WriteIndented=true}));Console.WriteLine(JsonSerializer.Serialize(audit));
    }
}
