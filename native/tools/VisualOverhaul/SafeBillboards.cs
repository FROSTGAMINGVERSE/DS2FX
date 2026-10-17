using SoulsFormats;
using System.IO.Compression;
using System.Text.Json;

// Exact, reviewed additions built from original zero-instance metadata.
sealed class SafeBillboards {
    static readonly JsonSerializerOptions Json=new(){IncludeFields=true};
    static readonly (string Map,string Name,string Model,short Flag)[] Policy=[
        ("m10_10_00_00","o00_9151","o00_9155",10655),
        ("m10_10_00_00","o00_9152","o00_9161",10655),
        ("m10_16_00_00","o00_0240_0138","o00_9155",16650),
        ("m10_23_00_00","o00_2010_0013","o00_9155",23650),
        ("m10_31_00_00","o31_9010_0045","o00_9156",0),
        ("m10_31_00_00","o31_9010_0049","o00_9162",0),
        ("m20_10_00_00","o10_1010_0004","o00_9161",10675),
        ("m20_10_00_00","o10_1010_0005","o00_9155",10675),
        ("m20_21_00_00","o00_0504_0001","o00_9155",0),
        ("m50_37_00_00","o00_1080_0001","o00_9152",0),
    ];
    static readonly Dictionary<string,string> Hashes=new(){
        ["o00_9152"]="e2cf334129ee7992cfa274da1119af0fae7bb7510a7ebe288ee5d99f75eb44f2",
        ["o00_9155"]="19b300b2b342b8d461caa4c28a576c7bdd70e1196ac96896e413d129e7ae3f80",
        ["o00_9156"]="cfcc9d9ec4c8ebbad0d1627e8a2c17e62422274405f0fb2133bb4af9bf389942",
        ["o00_9161"]="e0211ed9b4d5b1de234d59267029514786761b519ceff947aab514a3850dc86e",
        ["o00_9162"]="798448ec0528bcbea55542d1f64555abeaf0fb939176858d4b5ba6d7fdaa6700",
    };
    readonly Dictionary<string,byte[]> binders=new();
    public List<object> Receipts {get;}=new();
    static void Need(bool condition,string why){if(!condition)throw new InvalidDataException(why);}
    static string NonVisual(MSB2.Part.Object p){var j=JsonSerializer.SerializeToElement(p,Json);return string.Join('|',j.EnumerateObject().Where(f=>f.Name is not ("Name" or "ModelName" or "Position" or "Rotation" or "Scale" or "DrawGroups" or "DispGroups")).Select(f=>f.Name+":"+f.Value.GetRawText()));}
    public IEnumerable<(MSB2.Part.Object Part,byte[] Binder)> ForMap(string id,MSB2 source,MSB2 baseline,ZipArchive zip,GameArchive game,SafeScenery scenery){
        foreach(var item in Policy.Where(p=>p.Map==id)){
            var p=source.Parts.Objects.Single(p=>p.Name==item.Name);
            Need(!baseline.Parts.GetEntries().Any(q=>q.Name==item.Name),"Billboard is not additive "+item.Name);
            Need(p.ModelName==item.Model&&p.MapObjectInstanceParamID==0&&p.UnkT04==item.Flag&&!source.PartPoses.Any(q=>q.PartName==p.Name),"Billboard source contract changed "+p.Name);
            var normalized=(MSB2.Part.Object)p.DeepCopy();normalized.UnkT04=0;
            var template=baseline.Parts.Objects.FirstOrDefault(q=>q.MapObjectInstanceParamID==0&&q.UnkT04==0&&!baseline.PartPoses.Any(pose=>pose.PartName==q.Name)&&NonVisual(q)==NonVisual(normalized));
            Need(template!=null,"No matching original zero-instance metadata "+p.Name);
            var owned=(MSB2.Part.Object)template!.DeepCopy();owned.Name=p.Name;owned.ModelName=p.ModelName;owned.Position=p.Position;owned.Rotation=p.Rotation;owned.Scale=p.Scale;
            p.DrawGroups.CopyTo(owned.DrawGroups,0);p.DispGroups.CopyTo(owned.DispGroups,0);
            Need(NonVisual(owned)==NonVisual(template),"Billboard non-visual metadata changed "+p.Name);
            scenery.VerifyAdditionIdentity(p.Name,baseline);
            if(!binders.TryGetValue(p.ModelName,out var bytes)){bytes=HeideBackdrop.ReadReviewedBinder(p.ModelName,Hashes[p.ModelName],zip,game);binders.Add(p.ModelName,bytes);}
            Receipts.Add(new{map=id,source_name=p.Name,p.ModelName,source_unknown_flag=p.UnkT04,installed_unknown_flag=owned.UnkT04,original_template=template.Name,original_nonvisual_metadata_preserved=true,instance_id=0,model_sha256=Hashes[p.ModelName],original_empty_material_timing=true,original_render_config_only_mesh_path_change=true,literal_original_script_name_absent=true,script_files_checked=scenery.ScriptFilesChecked,scope="Source visuals only on an original zero-instance template; source area flags are not imported. Static four-vertex unlit card; visual quality, computed references and live visibility remain separate checks."});
            yield return(owned,bytes);
        }
    }
    public void Complete(string output){Need(Receipts.Count==Policy.Length,"Incomplete billboard additions");File.WriteAllText(Path.Combine(output,"billboard-admissions.json"),JsonSerializer.Serialize(Receipts,new JsonSerializerOptions{WriteIndented=true}));}
}
