using SoulsFormats;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;

static class VisualBuild {
    const string Prefix="SeekerOfFire_Seamless/";
    static readonly JsonSerializerOptions Json=new(){IncludeFields=true};
    static string Canon(object o)=>JsonSerializer.Serialize(o,o.GetType(),Json);
    static string CanonList<T>(IEnumerable<T> items)=>string.Join('\n',items.Select(o=>Canon(o!)));
    static string Hash(byte[] b)=>Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();
    static void Need(bool condition,string message){if(!condition)throw new InvalidDataException(message);}
    static byte[] ZipBytes(ZipArchiveEntry e){using var m=new MemoryStream();using var s=e.Open();s.CopyTo(m);return m.ToArray();}
    public static void Run(ZipArchive zip,GameArchive vanilla,string output,string gameDirectory){
        var scenery=new SafeScenery(zip,vanilla,gameDirectory);
        var billboards=new SafeBillboards();
        string pack=Path.Combine(output,"DS2FX - Visual Overhaul","Root","Game","DS2FX","Content","ds2-visual-overhaul");
        Directory.CreateDirectory(Path.Combine(pack,"assets"));
        var assets=new SortedDictionary<string,string>(StringComparer.Ordinal);
        var models=new Dictionary<string,(bool Eligible,byte[]? NewBytes,string Reason)>();
        var reports=new List<object>();new BXF4().Write(out _,out byte[] emptyData);
        foreach(var entry in zip.Entries.Where(e=>e.FullName.StartsWith(Prefix+"map/")&&e.FullName.EndsWith(".msb")).OrderBy(e=>e.FullName,StringComparer.Ordinal)){
            string relative=entry.FullName[Prefix.Length..],id=Path.GetFileNameWithoutExtension(relative);
            var source=MSB2.Read(ZipBytes(entry));var originalBytes=vanilla.Read(relative);var baseline=MSB2.Read(originalBytes);var candidate=MSB2.Read(originalBytes);
            Need(source.Format==MSB2.MSBFormat.DarkSouls2Scholar&&baseline.Format==source.Format,"Wrong map format");
            using var header=new BXF4Reader(vanilla.Read($"model/map/{id}.mapbhd"),emptyData);
            var availableGeometry=header.Files.Where(f=>f.Name.EndsWith(".flv",StringComparison.OrdinalIgnoreCase)||f.Name.EndsWith(".flv.dcx",StringComparison.OrdinalIgnoreCase))
                .Select(f=>Path.GetFileName(f.Name.Replace('\\','/')).Split('.')[0]).ToHashSet(StringComparer.Ordinal);
            var names=candidate.Parts.GetEntries().Select(p=>p.Name).ToHashSet(StringComparer.Ordinal);
            var basePieces=baseline.Parts.MapPieces.ToDictionary(p=>p.Name,StringComparer.Ordinal);
            var added=new List<object>();var skipped=new List<object>();var groups=new List<object>();
            foreach(var p in source.Parts.MapPieces){
                if(basePieces.TryGetValue(p.Name,out var old)){
                    // Extend only visibility; never move existing geometry away from its collision.
                    var target=candidate.Parts.MapPieces.Single(x=>x.Name==p.Name);
                    var before=Canon(target);
                    for(int i=0;i<4;i++){target.DrawGroups[i]|=p.DrawGroups[i];target.DispGroups[i]|=p.DispGroups[i];}
                    if(Canon(target)!=before)groups.Add(new{p.Name,kind="extend-visibility-groups",before=old.DrawGroups,after=target.DrawGroups});
                    continue;
                }
                if(!availableGeometry.Contains(p.ModelName)){skipped.Add(new{p.Name,p.ModelName,reason="geometry absent from vanilla map container"});continue;}
                if(names.Contains(p.Name)){skipped.Add(new{p.Name,p.ModelName,reason="name collision"});continue;}
                if(!candidate.Models.MapPieces.Any(m=>m.Name==p.ModelName)){
                    var model=source.Models.MapPieces.Single(m=>m.Name==p.ModelName);
                    candidate.Models.MapPieces.Add((MSB2.Model.MapPiece)model.DeepCopy());
                }
                var copy=(MSB2.Part.MapPiece)p.DeepCopy();copy.Name=NewName(p.Name);
                candidate.Parts.MapPieces.Add(copy);added.Add(new{source_name=p.Name,name=copy.Name,p.ModelName,kind="static-map-piece"});
            }
            foreach(var p in source.Parts.Objects.Where(p=>!names.Contains(p.Name))){
                if(p.MapObjectInstanceParamID!=0){skipped.Add(new{p.Name,p.ModelName,reason="nonzero gameplay instance parameter"});continue;}
                if(source.PartPoses.Any(x=>x.PartName==p.Name)){skipped.Add(new{p.Name,p.ModelName,reason="requires authored object pose"});continue;}
                if(!models.TryGetValue(p.ModelName,out var resource)){
                    string path=$"model/obj/{p.ModelName}.bnd";byte[]? vanillaBytes=null;
                    try{vanillaBytes=vanilla.Read(path);}catch(FileNotFoundException){ }
                    var modEntry=zip.GetEntry(Prefix+path);byte[]? modBytes=modEntry==null?null:ZipBytes(modEntry);
                    // Existing model replacements would also change vanilla objects elsewhere.
                    if(vanillaBytes!=null&&modBytes!=null&&Hash(vanillaBytes)!=Hash(modBytes))resource=(false,null,"would replace existing global model");
                    else{
                        var bytes=vanillaBytes??modBytes;
                        if(bytes==null||!BND4.Is(bytes))resource=(false,null,"missing model binder");
                        else{
                            var b=BND4.Read(bytes);
                            bool mesh=b.Files.Any(f=>f.Name.EndsWith(".flv",StringComparison.OrdinalIgnoreCase)||f.Name.EndsWith(".flv.dcx",StringComparison.OrdinalIgnoreCase));
                            // First prototype accepts self-contained mesh/texture binders only.
                            // No Havok, animation/configuration, material-time, effect or unknown files.
                            bool pure=mesh&&b.Files.All(f=>f.Name.EndsWith(".flv",StringComparison.OrdinalIgnoreCase)||f.Name.EndsWith(".flv.dcx",StringComparison.OrdinalIgnoreCase)||f.Name.EndsWith(".tpf",StringComparison.OrdinalIgnoreCase)||f.Name.EndsWith(".tpf.dcx",StringComparison.OrdinalIgnoreCase));
                            if(pure){foreach(var f in b.Files){if(f.Name.Contains(".flv")){var flver=FLVER2.Read(f.Bytes);Need(flver.Meshes.Count>0,"Empty visual mesh");}else _=TPF.Read(f.Bytes);}}
                            resource=(pure,pure&&vanillaBytes==null?bytes:null,pure?"mesh-texture-only":"contains collision/configuration/unknown dependencies");
                        }
                    }
                    models[p.ModelName]=resource;
                }
                if(!resource.Eligible){skipped.Add(new{p.Name,p.ModelName,reason=resource.Reason});continue;}
                if(!candidate.Models.Objects.Any(m=>m.Name==p.ModelName))candidate.Models.Objects.Add((MSB2.Model.Object)source.Models.Objects.Single(m=>m.Name==p.ModelName).DeepCopy());
                var copy=(MSB2.Part.Object)p.DeepCopy();copy.Name=NewName(p.Name);
                candidate.Parts.Objects.Add(copy);added.Add(new{source_name=p.Name,name=copy.Name,p.ModelName,kind="mesh-texture-only-decoration"});
                if(resource.NewBytes!=null)WriteAsset($"model/obj/{p.ModelName}.bnd",resource.NewBytes);
            }
            // Append curated additions after the original prototype pass, preserving
            // every existing prototype placement name and its order.
            foreach(var admission in scenery.ForMap(id,source,baseline)){
                var p=admission.Part;
                if(!candidate.Models.Objects.Any(m=>m.Name==p.ModelName))candidate.Models.Objects.Add((MSB2.Model.Object)source.Models.Objects.Single(m=>m.Name==p.ModelName).DeepCopy());
                var copy=(MSB2.Part.Object)p.DeepCopy();copy.Name=NewName(p.Name);
                candidate.Parts.Objects.Add(copy);added.Add(new{source_name=p.Name,name=copy.Name,p.ModelName,kind="audited-original-inert-contract-decoration"});
                skipped.RemoveAll(s=>JsonSerializer.SerializeToElement(s).GetProperty("Name").GetString()==p.Name);
                WriteAsset($"model/obj/{p.ModelName}.bnd",admission.Binder);
            }
            var backdrop=HeideBackdrop.Apply(id,source,baseline,candidate,zip,vanilla,output);
            if(backdrop!=null)WriteAsset("model/obj/o00_9155.bnd",backdrop);
            foreach(var admission in billboards.ForMap(id,source,baseline,zip,vanilla,scenery)){
                var p=admission.Part;
                if(!candidate.Models.Objects.Any(m=>m.Name==p.ModelName))candidate.Models.Objects.Add((MSB2.Model.Object)source.Models.Objects.Single(m=>m.Name==p.ModelName).DeepCopy());
                var copy=(MSB2.Part.Object)p.DeepCopy();copy.Name=NewName(p.Name);
                candidate.Parts.Objects.Add(copy);added.Add(new{source_name=p.Name,name=copy.Name,p.ModelName,kind="audited-static-billboard-decoration"});
                skipped.RemoveAll(s=>JsonSerializer.SerializeToElement(s).GetProperty("Name").GetString()==p.Name);
                WriteAsset($"model/obj/{p.ModelName}.bnd",admission.Binder);
            }
            if(added.Count==0&&groups.Count==0&&backdrop==null){reports.Add(new{id,changed=false,added,groups,skipped});continue;}
            var bytesOut=candidate.Write();var check=MSB2.Read(bytesOut);
            Verify(baseline,candidate,check,id);
            WriteAsset(relative,bytesOut);
            reports.Add(new{id,changed=true,baseline_sha256=Hash(originalBytes),output_sha256=Hash(bytesOut),added,groups,skipped,heide_backdrop_updated=backdrop!=null});
            Console.WriteLine($"{id}: +{added.Count} visual placements; {groups.Count} visibility extensions; {skipped.Count} excluded");
            string NewName(string sourceName){
                // SoulsFormats strips its display-only " {n}" suffix on write.
                // Give additions truly unique on-disk names, preserving all base names.
                string stem=sourceName.Split(" {",StringSplitOptions.None)[0].TrimEnd();int n=added.Count;
                string name;do{name=$"{stem}_ds2vo_{n++:D4}";}while(names.Contains(name));names.Add(name);return name;
            }
        }
        Need(assets.Keys.Any(p=>p.EndsWith(".msb")),"No visual changes built");
        scenery.Complete(output);
        billboards.Complete(output);
        File.WriteAllText(Path.Combine(pack,"files.tsv"),"DS2FX-CONTENT-2 ds2-visual-overhaul 0.1.3 shared-content-experimental\n"+string.Join('\n',assets.Select(p=>p.Value+"\t"+p.Key))+"\n");
        File.WriteAllText(Path.Combine(pack,"forge.ini"),"[Mod]\nName=DS2 Visual Overhaul (local prototype)\nDescription=Environment placements and extended landmark visibility. Vanilla gameplay sections preserved.\nCredits=FROST FX adaptation; Seeker of Fire by Aoi Yozora and credited contributors. Permission required for release.\n");
        File.WriteAllText(Path.Combine(output,"build-report.json"),JsonSerializer.Serialize(new{version="0.1.3",required_core="0.5.37-alpha.1",assets=assets.Count,maps=reports,curated_admissions=scenery.Receipts.Count,heide_backdrop_updates=1,billboard_additions=billboards.Receipts.Count,policy="Vanilla-based map composition; preserve 0.1.2 content. Append ten exact static billboards from original zero-instance metadata; source area flags, scripts and gameplay parameters are not imported."},new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"Built {assets.Count} assets; gameplay and semantic roundtrip checks passed for every changed map.");
        void WriteAsset(string path,byte[] bytes){string file=Path.Combine(pack,"assets",path.Replace('/',Path.DirectorySeparatorChar));Directory.CreateDirectory(Path.GetDirectoryName(file)!);if(assets.TryGetValue(path,out var prior))Need(prior==Hash(bytes),"Conflicting asset "+path);else{File.WriteAllBytes(file,bytes);assets.Add(path,Hash(bytes));}}
    }
    static void Verify(MSB2 baseline,MSB2 candidate,MSB2 check,string id){
        Need(check.Format==baseline.Format,"Format changed "+id);
        Need(CanonList(candidate.Parts.GetEntries())==CanonList(check.Parts.GetEntries()),"Part roundtrip changed "+id);
        Need(CanonList(candidate.Models.GetEntries())==CanonList(check.Models.GetEntries()),"Model roundtrip changed "+id);
        Need(CanonList(baseline.Events.GetEntries())==CanonList(check.Events.GetEntries()),"Events changed "+id);
        Need(CanonList(baseline.Regions.GetEntries())==CanonList(check.Regions.GetEntries()),"Regions changed "+id);
        Need(CanonList(baseline.PartPoses)==CanonList(check.PartPoses),"Poses changed "+id);
        Need(CanonList(baseline.Parts.Collisions)==CanonList(check.Parts.Collisions),"Collision changed "+id);
        Need(CanonList(baseline.Parts.Navmeshes)==CanonList(check.Parts.Navmeshes),"Navigation changed "+id);
        Need(CanonList(baseline.Parts.ConnectCollisions)==CanonList(check.Parts.ConnectCollisions),"Map connections changed "+id);
        HeideBackdrop.VerifyOriginalObjects(id,baseline,check);
        Need(CanonList(baseline.Models.GetEntries().Where(m=>m is not MSB2.Model.MapPiece and not MSB2.Model.Object))==CanonList(check.Models.GetEntries().Where(m=>m is not MSB2.Model.MapPiece and not MSB2.Model.Object)),"Gameplay models changed "+id);
        foreach(var p in baseline.Parts.MapPieces){var after=check.Parts.MapPieces.Single(q=>q.Name==p.Name);Need(after.ModelName==p.ModelName&&after.Position==p.Position&&after.Rotation==p.Rotation&&after.Scale==p.Scale,"Existing geometry transform changed "+id);}
    }
}
