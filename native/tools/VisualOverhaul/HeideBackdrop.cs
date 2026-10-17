using SoulsFormats;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;

// One reviewed replacement of a distant billboard, not a general object-change policy.
static class HeideBackdrop {
    static readonly JsonSerializerOptions Json=new(){IncludeFields=true};
    const string Map="m10_04_00_00", Name="o00_9150_0000", Model="o00_9155";
    const string BinderHash="19b300b2b342b8d461caa4c28a576c7bdd70e1196ac96896e413d129e7ae3f80";
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
    static void Need(bool b,string why){if(!b)throw new InvalidDataException(why);}
    static string Canon(object o)=>JsonSerializer.Serialize(o,o.GetType(),Json);
    static byte[] Bytes(ZipArchiveEntry e){using var s=e.Open();using var m=new MemoryStream();s.CopyTo(m);return m.ToArray();}
    static void VisualFields(MSB2.Part.Object from,MSB2.Part.Object to){
        to.ModelName=from.ModelName;to.Position=from.Position;to.Rotation=from.Rotation;to.Scale=from.Scale;
        from.DrawGroups.CopyTo(to.DrawGroups,0);from.DispGroups.CopyTo(to.DispGroups,0);
    }
    public static byte[]? Apply(string id,MSB2 source,MSB2 original,MSB2 candidate,ZipArchive zip,GameArchive game,string output){
        if(id!=Map)return null;
        var old=original.Parts.Objects.Single(p=>p.Name==Name);var s=source.Parts.Objects.Single(p=>p.Name==Name);
        Need(old.ModelName=="o00_9150"&&s.ModelName==Model&&old.MapObjectInstanceParamID==10043808&&s.MapObjectInstanceParamID==old.MapObjectInstanceParamID&&old.UnkT04==0&&s.UnkT04==0,"Heide backdrop identity changed");
        Need(!original.PartPoses.Any(p=>p.PartName==Name)&&!source.PartPoses.Any(p=>p.PartName==Name),"Heide backdrop has a pose");
        var normalized=(MSB2.Part.Object)s.DeepCopy();VisualFields(old,normalized);
        Need(Canon(normalized)==Canon(old),"Source changes non-visual Heide metadata");
        var bytes=ReadReviewedBinder(Model,BinderHash,zip,game);
        var target=candidate.Parts.Objects.Single(p=>p.Name==Name);VisualFields(s,target);
        if(!candidate.Models.Objects.Any(m=>m.Name==Model))candidate.Models.Objects.Add((MSB2.Model.Object)source.Models.Objects.Single(m=>m.Name==Model).DeepCopy());
        File.WriteAllText(Path.Combine(output,"heide-backdrop-admission.json"),JsonSerializer.Serialize(new{map=id,name=Name,original_model=old.ModelName,new_model=Model,binder_sha256=BinderHash,original_position=old.Position,source_position=s.Position,empty_original_material_timing=true,render_configuration_only_mesh_path_changes=true,original_non_visual_fields_preserved=true,original_instance_type_verified_by_safe_scenery=true,scope="Local visual billboard replacement and source placement; no collision, motion, scripts or gameplay PARAM import. Requires live viewpoint verification."},new JsonSerializerOptions{WriteIndented=true,IncludeFields=true}));
        return bytes;
    }
    public static byte[] ReadReviewedBinder(string model,string expected,ZipArchive zip,GameArchive game){
        var bytes=Bytes(zip.GetEntry("SeekerOfFire_Seamless/model/obj/"+model+".bnd")??throw new FileNotFoundException(model));
        Need(Hash(bytes)==expected,"Unreviewed billboard binder "+model);
        bool exists=false;try{_ =game.Read("model/obj/"+model+".bnd");exists=true;}catch(FileNotFoundException){}
        Need(!exists,"Heide binder would replace an original global model");
        var b=BND4.Read(bytes);var baseline=BND4.Read(game.Read("model/obj/o00_9150.bnd"));
        Need(b.Files.Count==4&&b.Files.Select(f=>f.Name).Order().SequenceEqual(new[]{model+".flv","o00_9150.mte.dcx","o00_9151.acb","o00_9151_d.tpf"}.Order()),"Billboard binder dependencies changed "+model);
        var mte=b.Files.Single(f=>f.Name=="o00_9150.mte.dcx").Bytes;
        Need(mte.SequenceEqual(baseline.Files.Single(f=>f.Name=="o00_9150.mte.dcx").Bytes)&&Convert.ToHexString(DCX.Decompress(mte))=="4D54450002000000","Heide material timing is not the original empty table");
        var acb=ACB.Read(b.Files.Single(f=>f.Name=="o00_9151.acb").Bytes);
        var originalAcb=ACB.Read(baseline.Files.Single(f=>f.Name=="o00_9150.acb").Bytes);
        Need(acb.Assets.Count==1&&Canon(acb).Replace("o00_9151","o00_9150",StringComparison.Ordinal)==Canon(originalAcb),"Heide render configuration changes more than the mesh path");
        var flver=FLVER2.Read(b.Files.Single(f=>f.Name==model+".flv").Bytes);
        var oldFlver=FLVER2.Read(baseline.Files.Single(f=>f.Name=="o00_9150.flv").Bytes);
        Need(flver.Meshes.Sum(m=>m.Vertices.Count)==4&&flver.Meshes.All(m=>m.Dynamic==0)&&flver.Materials.Count==1&&flver.Materials[0].MTD==oldFlver.Materials.Single().MTD,"Heide is not the reviewed static alpha billboard");
        var textures=TPF.Read(b.Files.Single(f=>f.Name=="o00_9151_d.tpf").Bytes);
        Need(textures.Textures.Count==1&&textures.Textures[0].Name=="o00_9151_d","Heide texture changed");
        return bytes;
    }
    public static void VerifyOriginalObjects(string id,MSB2 original,MSB2 check){
        Need(check.Parts.Objects.Count>=original.Parts.Objects.Count,"Objects removed "+id);
        for(int i=0;i<original.Parts.Objects.Count;i++){
            var old=original.Parts.Objects[i];var after=(MSB2.Part.Object)check.Parts.Objects[i].DeepCopy();
            if(id==Map&&old.Name==Name){Need(after.ModelName==Model,"Heide model not routed");VisualFields(old,after);}
            Need(Canon(old)==Canon(after),"Existing object metadata changed "+id+"/"+old.Name);
        }
    }
}
