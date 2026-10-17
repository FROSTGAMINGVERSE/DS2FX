using SoulsFormats;
using Org.BouncyCastle.Crypto.Parameters;
using Org.BouncyCastle.Security;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;

// Placement-specific admission. Do not turn the research shortlist into a blanket rule.
sealed class SafeScenery {
    const string Prefix="SeekerOfFire_Seamless/";
    const string OriginalRegulationHash="be2a1860eeb59b548e4b81b433fdd1db54f44dc8f2114e06c49d45f7d49af6b5";
    static readonly JsonSerializerOptions Json=new(){IncludeFields=true};
    static readonly Dictionary<string,string[]> Policy=new(){
        ["m10_04_00_00"]=["o00_9150_0003","o00_9150_0004","o04_9000_0002","o04_9000_0003"],
        ["m10_18_00_00"]=["o00_0210_0012"],
        ["m10_31_00_00"]=["o00_0100_0013","o00_0100_0014","o00_0100_0015","o31_9010_0030","o31_9010_0031","o31_9010_0060","o31_9010_0061"],
        ["m20_21_00_00"]=["o00_0100_0002_1_1_1_1_1_1_1"],
        ["m50_36_00_00"]=["o36_1100_0004"],
    };
    static readonly Dictionary<string,string> ModelHashes=new(){
        ["o00_0003"]="a0aa91957cd4c2e93f3aba7caf1343c4ffa0b84e2533872a07907929e0b012e0",
        ["o00_0025"]="d6fcc3faad9770622754651abaa277cedcc56526a66c85b31db3cf41a55727be",
        ["o00_0036"]="adcf0e456efe4cdcc43656ec49e045617dca30f09797743598ed3a01efcd1674",
        ["o00_0046"]="3005c781cb29f33eb51af1060b0afa4bb147636f71715df8b2f056232ffc01a8",
        ["o00_0047"]="ea8b038d4b3cd2021c9c954dc16db7e884a8809981d2bec098229fb5eef6eb78",
        ["o60_0052"]="640d946e6b2ad578e86fb1185f20738bc637c40afec1864b4e3e9cbb69862a74",
        ["o60_0053"]="5244b0cf83a1098942167275216bad67beb062ad6b101991134bcaeb72351205",
        ["o80_0019"]="7b2aa9994115f64d82b6e436cde0b142b73ec4bff884a5716b67ff3bcbd03a4a",
    };
    readonly ZipArchive zip;readonly GameArchive game;readonly PARAMDEF instanceDef;readonly PARAM.Row originalType;
    readonly List<byte[]> originalScripts=new();public List<object> Receipts {get;}=new();
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
    static void Need(bool condition,string message){if(!condition)throw new InvalidDataException(message);}
    static byte[] Bytes(ZipArchiveEntry e){using var s=e.Open();using var m=new MemoryStream();s.CopyTo(m);return m.ToArray();}
    static string Fields(PARAM.Row r)=>JsonSerializer.Serialize(r.Cells.Select(c=>new{c.Def.InternalName,c.Value}),Json);
    static long Number(PARAM.Row r,string key)=>Convert.ToInt64(r.Cells.Single(c=>c.Def.InternalName==key).Value);
    static string Schemas(){for(var d=new DirectoryInfo(AppContext.BaseDirectory);d!=null;d=d.Parent){var path=Path.Combine(d.FullName,"vendor/Paramdex/DS2S/Defs");if(Directory.Exists(path))return path;}throw new DirectoryNotFoundException("DS2S definitions not found");}
    static PARAMDEF Definition(string name,string expected){var file=Path.Combine(Schemas(),name);Need(Hash(File.ReadAllBytes(file))==expected,"Changed object PARAM definition "+name);return PARAMDEF.XmlDeserialize(file);}
    static PARAM Parse(byte[] bytes,PARAMDEF def){var p=PARAM.Read(bytes);p.ApplyParamdef(def);return p;}
    static BND4 Regulation(string file){
        var bytes=File.ReadAllBytes(file);if(BND4.Is(bytes))return BND4.Read(bytes);
        var iv=new byte[16];iv[0]=0x80;Array.Copy(bytes,0,iv,1,11);iv[15]=1;
        var cipher=CipherUtilities.GetCipher("AES/CTR/NoPadding");cipher.Init(false,new ParametersWithIV(new KeyParameter(new byte[]{0x40,0x17,0x81,0x30,0xDF,0x0A,0x94,0x54,0x33,0x09,0xE1,0x71,0xEC,0xBF,0x25,0x4C}),iv));
        return BND4.Read(cipher.DoFinal(bytes,32,bytes.Length-32));
    }
    static PARAMDEF TypeDefinition()=>Definition("MAP_OBJECT_INSTANCE_TYPE_PARAM.xml","23e785fdba7b9b8a01f9f30b72258d38c8796a207175d915479bc0c2d3f8a332");
    static PARAM.Row OriginalType(string gameDirectory){
        var file=Path.Combine(gameDirectory,"enc_regulation.bnd.dcx");Need(Hash(File.ReadAllBytes(file))==OriginalRegulationHash,"Original regulation changed; require clean Root Builder state and a new audit");
        var entry=Regulation(file).Files.Single(f=>Path.GetFileName(f.Name.Replace('\\','/')).Equals("MapObjectInstanceTypeParam.param",StringComparison.OrdinalIgnoreCase));
        return Parse(entry.Bytes,TypeDefinition()).Rows.Single(r=>r.ID==1607);
    }
    public static void VerifyEffectiveTypes(string regulation,string gameDirectory,string output){
        var original=OriginalType(gameDirectory);var b=Regulation(regulation);
        var entry=b.Files.SingleOrDefault(f=>Path.GetFileName(f.Name.Replace('\\','/')).Equals("MapObjectInstanceTypeParam.param",StringComparison.OrdinalIgnoreCase));
        if(entry!=null)Need(Fields(Parse(entry.Bytes,TypeDefinition()).Rows.Single(r=>r.ID==1607))==Fields(original),"Effective regulation modifies scenery type 1607");
        File.WriteAllText(Path.Combine(output,"type-verification.json"),JsonSerializer.Serialize(new{regulation,sha256=Hash(File.ReadAllBytes(regulation)),type_id=1607,type_present_in_override=entry!=null,result=entry==null?"inherits original type; check loose PARAM overrides separately":"exact original type preserved"},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine("Effective regulation preserves scenery type 1607: "+regulation);
    }
    public SafeScenery(ZipArchive zip,GameArchive game,string gameDirectory){
        this.zip=zip;this.game=game;instanceDef=Definition("MAP_OBJECT_INSTANCE_PARAM.xml","c2ef8abb4921ae9e4f5a81d5ec27b55ae997428b3d01b64a41d43c3f07d0384d");originalType=OriginalType(gameDirectory);
        var sourceType=Parse(Bytes(zip.GetEntry(Prefix+"Param/MapObjectInstanceTypeParam.param")!),TypeDefinition()).Rows.Single(r=>r.ID==1607);
        Need(Fields(sourceType)==Fields(originalType),"Source changes inert object type");
        // A literal-name check supplements the preserved vanilla script/map contracts.
        // It is not a decompiler or proof against computed runtime references.
        foreach(var e in zip.Entries.Where(e=>e.FullName.StartsWith(Prefix+"ezstate/")&&!e.FullName.EndsWith('/'))){
            try{var bytes=game.Read(e.FullName[Prefix.Length..]);if(DCX.Is(bytes))bytes=DCX.Decompress(bytes);originalScripts.Add(bytes);}catch(FileNotFoundException){}
        }
    }
    static string NonVisual(MSB2.Part.Object p){
        var o=JsonSerializer.SerializeToElement(p,Json);return string.Join('|',o.EnumerateObject().Where(f=>f.Name is not ("Name" or "ModelName" or "Position" or "Rotation" or "Scale" or "DrawGroups" or "DispGroups")).Select(f=>f.Name+":"+f.Value.GetRawText()));
    }
    public int ScriptFilesChecked=>originalScripts.Count;
    public void VerifyAdditionIdentity(string name,MSB2 baseline){
        Need(!originalScripts.Any(s=>s.AsSpan().IndexOf(System.Text.Encoding.UTF8.GetBytes(name))>=0||s.AsSpan().IndexOf(System.Text.Encoding.Unicode.GetBytes(name))>=0),"Original script names an admitted addition: "+name);
        Need(!JsonSerializer.Serialize(baseline.Events.GetEntries(),Json).Contains('"'+name+'"',StringComparison.Ordinal),"Original map event names an addition: "+name);
    }
    public IEnumerable<(MSB2.Part.Object Part,byte[] Binder)> ForMap(string id,MSB2 source,MSB2 baseline){
        if(!Policy.TryGetValue(id,out var names))yield break;
        string paramPath=$"Param/mapobjectinstanceparam_{id}.param";
        var sourceRows=Parse(Bytes(zip.GetEntry(Prefix+paramPath)!),instanceDef).Rows.ToDictionary(r=>r.ID);var originalRows=Parse(game.Read(paramPath),instanceDef).Rows.ToDictionary(r=>r.ID);
        foreach(var name in names){
            var p=source.Parts.Objects.Single(p=>p.Name==name);
            Need(!baseline.Parts.GetEntries().Any(q=>q.Name==name),"Admission is not additive: "+name);
            Need(p.MapObjectInstanceParamID!=0&&p.UnkT04==0&&!source.PartPoses.Any(q=>q.PartName==name),"Non-static/posed object: "+name);
            var row=sourceRows[p.MapObjectInstanceParamID];var old=originalRows[p.MapObjectInstanceParamID];
            Need(Fields(row)==Fields(old)&&Number(row,"Map Object Instance Type ID")==1607&&Number(row,"Itemlot ID")==0&&Number(row,"Param ID")==0,"Gameplay instance differs: "+name);
            var templates=baseline.Parts.Objects.Where(q=>q.MapObjectInstanceParamID==p.MapObjectInstanceParamID&&q.UnkT04==0&&!baseline.PartPoses.Any(pose=>pose.PartName==q.Name)).ToArray();
            Need(templates.Length>0,"Object has no original inert template: "+name);
            // Copy only visual placement fields. Unknown source metadata is not
            // imported, even when the source's instance row matches vanilla.
            var owned=(MSB2.Part.Object)templates[0].DeepCopy();owned.Name=p.Name;owned.ModelName=p.ModelName;owned.Position=p.Position;owned.Rotation=p.Rotation;owned.Scale=p.Scale;
            p.DrawGroups.CopyTo(owned.DrawGroups,0);p.DispGroups.CopyTo(owned.DispGroups,0);
            Need(NonVisual(owned)==NonVisual(templates[0]),"Owned decoration changes original inert metadata: "+name);
            string path=$"model/obj/{p.ModelName}.bnd";bool originalModel=false;try{_ =game.Read(path);originalModel=true;}catch(FileNotFoundException){}
            Need(!originalModel,"Would replace an original global model: "+path);
            var bytes=Bytes(zip.GetEntry(Prefix+path)??throw new FileNotFoundException(path));Need(ModelHashes.TryGetValue(p.ModelName,out var expected)&&Hash(bytes)==expected,"Unreviewed model: "+path);
            var binder=BND4.Read(bytes);int vertices=0;
            foreach(var f in binder.Files){var normalized=f.Name.ToLowerInvariant();if(normalized.EndsWith(".flv")||normalized.EndsWith(".flv.dcx"))vertices+=FLVER2.Read(f.Bytes).Meshes.Sum(m=>m.Vertices.Count);else if(normalized.EndsWith(".tpf")||normalized.EndsWith(".tpf.dcx"))_ =TPF.Read(f.Bytes);else throw new InvalidDataException("Non-render dependency in "+path);}
            Need(vertices>0,"Empty model: "+path);
            VerifyAdditionIdentity(name,baseline);
            Receipts.Add(new{map=id,source_name=name,p.ModelName,p.MapObjectInstanceParamID,p.UnkT04,original_template=templates[0].Name,model_sha256=Hash(bytes),instance_and_type_match=true,non_visual_fields_match_original_template=true,source_unknown_metadata_imported=false,literal_original_script_name_absent=true,script_files_checked=originalScripts.Count,scope="Additive mesh/texture-only object built from original non-destructive metadata; only source transforms/model/visibility copied. No gameplay PARAM/script import. Computed references and live visual quality remain separate checks."});
            yield return(owned,bytes);
        }
    }
    public void Complete(string output){Need(Receipts.Count==14,"Incomplete safe scenery admission");File.WriteAllText(Path.Combine(output,"safe-scenery-admissions.json"),JsonSerializer.Serialize(Receipts,new JsonSerializerOptions{WriteIndented=true}));}
}
