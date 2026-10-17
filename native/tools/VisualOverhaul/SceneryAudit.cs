using SoulsFormats;
using Org.BouncyCastle.Crypto.Parameters;
using Org.BouncyCastle.Security;
using System.IO.Compression;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

// Read-only evidence export. No automatic policy relaxation or installation.
static class SceneryAudit {
    const string Prefix="SeekerOfFire_Seamless/";
    static readonly JsonSerializerOptions Options=new(){WriteIndented=true,IncludeFields=true};
    static byte[] Bytes(ZipArchiveEntry e){using var s=e.Open();using var m=new MemoryStream();s.CopyTo(m);return m.ToArray();}
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
    static object[] Values(PARAM.Row row)=>row.Cells.Select(c=>(object)new{field=c.Def.InternalName,value=c.Value}).ToArray();
    static long Number(PARAM.Row? row,string key)=>row==null?-1:Convert.ToInt64(row.Cells.Single(c=>c.Def.InternalName==key).Value);
    static void Save(string file,object value)=>File.WriteAllText(file,JsonSerializer.Serialize(value,Options));
    public static void Run(ZipArchive zip,GameArchive game,string output,string gameDirectory){
        string? schemas=null;for(var d=new DirectoryInfo(AppContext.BaseDirectory);d!=null;d=d.Parent){var test=Path.Combine(d.FullName,"vendor","Paramdex","DS2S");if(Directory.Exists(Path.Combine(test,"Defs"))){schemas=test;break;}}
        if(schemas==null)throw new IOException("Run the tool in the DS2FX source tree with vendor/Paramdex/DS2S available.");
        var instanceDef=PARAMDEF.XmlDeserialize(Path.Combine(schemas,"Defs","MAP_OBJECT_INSTANCE_PARAM.xml"));
        var typeDef=PARAMDEF.XmlDeserialize(Path.Combine(schemas,"Defs","MAP_OBJECT_INSTANCE_TYPE_PARAM.xml"));
        var labels=File.ReadLines(Path.Combine(schemas,"Names","MapObjectInstanceTypeParam.txt")).Where(s=>s.Contains(' ')).ToDictionary(s=>int.Parse(s[..s.IndexOf(' ')]),s=>s[(s.IndexOf(' ')+1)..]);
        PARAM Parse(string relative,PARAMDEF def){var e=zip.GetEntry(Prefix+relative)??throw new FileNotFoundException(relative);var p=PARAM.Read(Bytes(e));p.ApplyParamdef(def);return p;}
        var types=Parse("Param/MapObjectInstanceTypeParam.param",typeDef).Rows.ToDictionary(r=>r.ID);
        Dictionary<int,PARAM.Row>? vanillaTypes=null;
        string? typeOrigin=null,typeOriginHash=null;
        try{var vt=PARAM.Read(game.Read("Param/MapObjectInstanceTypeParam.param"));vt.ApplyParamdef(typeDef);vanillaTypes=vt.Rows.ToDictionary(r=>r.ID);}catch(FileNotFoundException){}
        if(vanillaTypes==null){
            // Original regulation is a loose game file, not a GameData entry.
            // Read/decrypt in memory only; do not export or change regulation.
            var path=Path.Combine(gameDirectory,"enc_regulation.bnd.dcx");
            var original=File.ReadAllBytes(path);typeOriginHash=Hash(original);
            var bytes=original;
            if(!BND4.Is(bytes)){
                var iv=new byte[16];iv[0]=0x80;Array.Copy(bytes,0,iv,1,11);iv[15]=1;
                var cipher=CipherUtilities.GetCipher("AES/CTR/NoPadding");
                cipher.Init(false,new ParametersWithIV(new KeyParameter(new byte[]{0x40,0x17,0x81,0x30,0xDF,0x0A,0x94,0x54,0x33,0x09,0xE1,0x71,0xEC,0xBF,0x25,0x4C}),iv));
                bytes=cipher.DoFinal(bytes,32,bytes.Length-32);
            }
            var b=BND4.Read(bytes);
            var entries=b.Files.Where(f=>Path.GetFileName(f.Name.Replace('\\','/')).Equals("MapObjectInstanceTypeParam.param",StringComparison.OrdinalIgnoreCase)).ToArray();
            if(entries.Length>1)throw new InvalidDataException("Ambiguous original object type PARAM");
            if(entries.Length==1){var vt=PARAM.Read(entries[0].Bytes);vt.ApplyParamdef(typeDef);vanillaTypes=vt.Rows.ToDictionary(r=>r.ID);typeOrigin="original loose regulation";}
        }else typeOrigin="GameData loose PARAM entry";
        var models=new Dictionary<string,ModelEvidence>(StringComparer.Ordinal);
        var placements=new List<object>();var existingChanges=new List<object>();Directory.CreateDirectory(Path.Combine(output,"models"));
        foreach(var entry in zip.Entries.Where(e=>e.FullName.StartsWith(Prefix+"map/")&&e.FullName.EndsWith(".msb")).OrderBy(e=>e.FullName)){
            string relative=entry.FullName[Prefix.Length..],id=Path.GetFileNameWithoutExtension(relative);
            var source=MSB2.Read(Bytes(entry));var baseline=MSB2.Read(game.Read(relative));
            var old=baseline.Parts.GetEntries().Select(p=>p.Name).ToHashSet(StringComparer.Ordinal);
            string instancePath=$"Param/mapobjectinstanceparam_{id}.param";
            var param=Parse(instancePath,instanceDef).Rows.ToDictionary(r=>r.ID);
            Dictionary<int,PARAM.Row>? vanillaParam=null;
            try{var vp=PARAM.Read(game.Read(instancePath));vp.ApplyParamdef(instanceDef);vanillaParam=vp.Rows.ToDictionary(r=>r.ID);}catch(FileNotFoundException){}
            var instanceLabels=File.ReadLines(Path.Combine(schemas,"Names",$"mapobjectinstanceparam_{id}.txt")).Where(s=>s.Contains(' ')).ToDictionary(s=>int.Parse(s[..s.IndexOf(' ')]),s=>s[(s.IndexOf(' ')+1)..]);
            foreach(var p in source.Parts.Objects.Where(p=>!old.Contains(p.Name))){
                if(!models.TryGetValue(p.ModelName,out var model)){model=Inspect(p.ModelName,zip,game,output);models.Add(p.ModelName,model);}
                param.TryGetValue(p.MapObjectInstanceParamID,out var row);
                PARAM.Row? vanillaRow=null;vanillaParam?.TryGetValue(p.MapObjectInstanceParamID,out vanillaRow);
                int type=(int)Number(row,"Map Object Instance Type ID");labels.TryGetValue(type,out var label);types.TryGetValue(type,out var typeRow);
                PARAM.Row? vanillaTypeRow=null;vanillaTypes?.TryGetValue(type,out vanillaTypeRow);
                long itemlot=Number(row,"Itemlot ID"),special=Number(row,"Param ID");
                bool pose=source.PartPoses.Any(q=>q.PartName==p.Name);
                // Labels are supporting evidence, not authoritative proof of inertness.
                var interactionLabel=label?.Replace("non-destructive","",StringComparison.OrdinalIgnoreCase);
                bool explicitInteraction=interactionLabel!=null&&new[]{"bonfire","treasure","ladder","lever","switch","door","brazier","light stand","corpse","item_","bridge","destroy","destruct","durab","barrel","vase","wooden box"}.Any(k=>interactionLabel.Contains(k,StringComparison.OrdinalIgnoreCase));
                string verdict=itemlot>0?"gameplay-loot":explicitInteraction?"gameplay-behavior":pose?"pose-dependent":model.MeshAndRenderOnly&&p.MapObjectInstanceParamID==0?"visual-dependency-candidate":model.MeshAndRenderOnly&&row==null?"unresolved-instance-candidate":model.MeshAndRenderOnly?"static-conversion-candidate":model.HasPhysics?"physics-or-animation-review":"unresolved-dependency";
                instanceLabels.TryGetValue(p.MapObjectInstanceParamID,out var instanceLabel);
                placements.Add(new{map=id,p.Name,p.ModelName,p.MapObjectInstanceParamID,p.UnkT04,p.Position,p.Rotation,p.Scale,p.DrawGroups,p.DispGroups,source_instance_present=row!=null,type_id=type,type_label=label,instance_label=instanceLabel,itemlot_id=itemlot,special_param_id=special,instance_fields=row==null?null:Values(row),type_fields=typeRow==null?null:Values(typeRow),vanilla_type_present=vanillaTypeRow!=null,type_matches_vanilla=typeRow!=null&&vanillaTypeRow!=null&&JsonSerializer.Serialize(Values(typeRow))==JsonSerializer.Serialize(Values(vanillaTypeRow)),vanilla_instance_present=vanillaRow!=null,instance_matches_vanilla=row!=null&&vanillaRow!=null&&JsonSerializer.Serialize(Values(row))==JsonSerializer.Serialize(Values(vanillaRow)),baseline_instance_uses=baseline.Parts.Objects.Where(q=>p.MapObjectInstanceParamID!=0&&q.MapObjectInstanceParamID==p.MapObjectInstanceParamID).Select(q=>new{q.Name,q.ModelName,q.UnkT04}),requires_pose=pose,classification=verdict,model.MeshAndRenderOnly,model.HasPhysics,model.HasMotion,model.UnknownExtensions,model.ChangedGlobalModel});
            }
            var originalObjects=baseline.Parts.Objects.ToDictionary(p=>p.Name,StringComparer.Ordinal);
            foreach(var p in source.Parts.Objects){
                if(!originalObjects.TryGetValue(p.Name,out var original))continue;
                var a=JsonSerializer.SerializeToElement(original,original.GetType(),Options);var b=JsonSerializer.SerializeToElement(p,p.GetType(),Options);
                var changed=b.EnumerateObject().Where(f=>!a.TryGetProperty(f.Name,out var oldValue)||oldValue.GetRawText()!=f.Value.GetRawText()).Select(f=>f.Name).ToArray();
                if(changed.Length==0)continue;
                if(!models.TryGetValue(p.ModelName,out var model)){model=Inspect(p.ModelName,zip,game,output);models.Add(p.ModelName,model);}
                param.TryGetValue(p.MapObjectInstanceParamID,out var row);PARAM.Row? vanillaRow=null;vanillaParam?.TryGetValue(p.MapObjectInstanceParamID,out vanillaRow);
                int type=(int)Number(row,"Map Object Instance Type ID");labels.TryGetValue(type,out var label);types.TryGetValue(type,out var typeRow);PARAM.Row? vanillaTypeRow=null;vanillaTypes?.TryGetValue(type,out vanillaTypeRow);
                bool instanceMatch=row!=null&&vanillaRow!=null&&JsonSerializer.Serialize(Values(row))==JsonSerializer.Serialize(Values(vanillaRow));
                bool typeMatch=typeRow!=null&&vanillaTypeRow!=null&&JsonSerializer.Serialize(Values(typeRow))==JsonSerializer.Serialize(Values(vanillaTypeRow));
                bool pose=source.PartPoses.Any(q=>q.PartName==p.Name)||baseline.PartPoses.Any(q=>q.PartName==p.Name);
                bool renderFieldsOnly=changed.All(f=>f is "DrawGroups" or "DispGroups");
                bool visualFieldsOnly=changed.All(f=>f is "DrawGroups" or "DispGroups" or "Position" or "Rotation" or "Scale" or "ModelName");
                bool inertContract=type==1607&&instanceMatch&&typeMatch&&Number(row,"Itemlot ID")==0&&Number(row,"Param ID")==0;
                string review=renderFieldsOnly&&!model.ChangedGlobalModel&&!pose?"visibility-only-review":visualFieldsOnly&&model.MeshAndRenderOnly&&!model.ChangedGlobalModel&&!pose&&inertContract?"existing-visual-change-candidate":"retain-original-object";
                existingChanges.Add(new{map=id,p.Name,original_model=original.ModelName,source_model=p.ModelName,changed_fields=changed,original_position=original.Position,source_position=p.Position,original_scale=original.Scale,source_scale=p.Scale,original_instance=original.MapObjectInstanceParamID,source_instance=p.MapObjectInstanceParamID,p.UnkT04,type_id=type,type_label=label,itemlot_id=Number(row,"Itemlot ID"),special_param_id=Number(row,"Param ID"),instance_matches_vanilla=instanceMatch,type_matches_vanilla=typeMatch,requires_pose=pose,model.MeshAndRenderOnly,model.HasPhysics,model.HasMotion,model.ChangedGlobalModel,review});
            }
        }
        Save(Path.Combine(output,"placements.json"),placements);
        Save(Path.Combine(output,"existing-object-changes.json"),existingChanges);
        Save(Path.Combine(output,"schema-provenance.json"),new{definitions=new[]{"MAP_OBJECT_INSTANCE_PARAM.xml","MAP_OBJECT_INSTANCE_TYPE_PARAM.xml"}.Select(n=>new{name=n,sha256=Hash(File.ReadAllBytes(Path.Combine(schemas,"Defs",n)))}),labels_sha256=Hash(File.ReadAllBytes(Path.Combine(schemas,"Names","MapObjectInstanceTypeParam.txt"))),original_type_origin=typeOrigin,original_type_origin_sha256=typeOriginHash,scope="Read-only candidate evidence; no proof of runtime gameplay neutrality. Nonzero IDs require static conversion or further behavior evidence. Mesh/config-only candidates still require route and visual review."});
        Console.WriteLine($"Classified {placements.Count} added and {existingChanges.Count} modified existing objects across 28 maps; inspected {models.Count} unique model binders. No deployed content changed.");
        SceneryContainers.Run(zip,game,output);
    }
    sealed record ModelEvidence(bool MeshAndRenderOnly,bool HasPhysics,bool HasMotion,string[] UnknownExtensions,bool ChangedGlobalModel);
    static ModelEvidence Inspect(string id,ZipArchive zip,GameArchive game,string output){
        string path=$"model/obj/{id}.bnd";byte[]? original=null;try{original=game.Read(path);}catch(FileNotFoundException){}
        var entry=zip.GetEntry(Prefix+path);var supplied=entry==null?null:Bytes(entry);var bytes=supplied??original;
        if(bytes==null){Save(Path.Combine(output,"models",id+".json"),new{id,missing=true});return new(false,false,false,new[]{"missing"},false);}
        var b=BND4.Read(bytes);var configurations=new List<object>();var meshes=new List<object>();var ext=new HashSet<string>();bool motion=false,physicsReference=false;
        foreach(var f in b.Files){
            string name=f.Name.Replace('\\','/').ToLowerInvariant();string extension=Path.GetExtension(name.EndsWith(".dcx")?name[..^4]:name);ext.Add(extension);
            if(extension==".acb"){
                try{
                    var acb=ACB.Read(f.Bytes);motion|=acb.Assets.Any(a=>a.Type==ACB.AssetType.Motion);
                    foreach(var asset in acb.Assets){
                        var dependency=(asset.RelativePath??asset.AbsolutePath??"").ToLowerInvariant();
                        if(dependency.EndsWith(".hkt")||dependency.EndsWith(".hkx")||dependency.EndsWith(".clm"))physicsReference=true;
                        else if(asset.Type is ACB.AssetType.General or ACB.AssetType.PWV)ext.Add(".acb-unresolved-reference");
                    }
                    configurations.Add(new{f.Name,assets=acb.Assets.Select(a=>JsonSerializer.SerializeToElement(a,a.GetType(),Options))});
                }catch(InvalidDataException error){
                    ext.Add(".acb-unsupported");
                    configurations.Add(new{f.Name,parse_error=error.Message,scope="Unparsed render metadata; candidate remains unresolved."});
                }
            }else if(extension==".flv"){
                var m=FLVER2.Read(f.Bytes);var v=m.Meshes.SelectMany(q=>q.Vertices).Select(v=>v.Position).ToArray();
                var min=v.Length==0?Vector3.Zero:v.Aggregate(new Vector3(float.PositiveInfinity),Vector3.Min);var max=v.Length==0?Vector3.Zero:v.Aggregate(new Vector3(float.NegativeInfinity),Vector3.Max);
                meshes.Add(new{f.Name,vertices=v.Length,min,max,size=max-min,bones=m.Bones.Select(q=>q.Name),materials=m.Materials.Select(q=>new{q.Name,q.MTD,texture_paths=q.Textures.Select(t=>t.Path)}),dynamic=m.Meshes.Select(q=>q.Dynamic).Distinct()});
                // Bound point clouds for shape identification, not a game-rendering substitute.
                if(id.StartsWith("o")&&v.Length>0){var cloud=v.Where((_,i)=>i%Math.Max(1,v.Length/12000)==0).Select(p=>new[]{p.X,p.Y,p.Z});Save(Path.Combine(output,"models",id+"-"+meshes.Count+"-points.json"),cloud);}
            }else if(extension==".tpf")_ = TPF.Read(f.Bytes);
        }
        bool physics=physicsReference||ext.Contains(".hkx")||ext.Contains(".clm");motion|=ext.Any(e=>e is ".nmb" or ".nsa" or ".tae" or ".anibnd");
        var unknown=ext.Except(new[]{".flv",".tpf",".acb"}).Order().ToArray();bool pure=meshes.Count>0&&b.Files.Any(f=>Path.GetExtension(f.Name).Equals(".flv",StringComparison.OrdinalIgnoreCase)&&FLVER2.Read(f.Bytes).Meshes.Any(m=>m.Vertices.Count>0))&&unknown.Length==0&&!motion&&!physics;
        bool changed=original!=null&&supplied!=null&&Hash(original)!=Hash(supplied);
        var evidence=new ModelEvidence(pure,physics,motion,unknown,changed);
        Save(Path.Combine(output,"models",id+".json"),new{id,path,source=supplied!=null?"seeker":"vanilla",sha256=Hash(bytes),vanilla_sha256=original==null?null:Hash(original),files=b.Files.Select(f=>new{f.Name,sha256=Hash(f.Bytes),bytes=f.Bytes.Length}),evidence,meshes,configurations});return evidence;
    }
}
