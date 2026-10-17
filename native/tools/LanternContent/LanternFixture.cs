using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using SoulsFormats;

// Local-only research fixture. It deliberately leaves probabilistic torch loot unchanged.
// The fixture is NOT a complete lantern release and is never installed by this tool.
static class LanternFixture
{
    const int Oil=60425000, Lantern=60425001, Light=51, Torch=60420000;
    static readonly FieldInfo Offset=typeof(PARAM.Row).GetField("DataOffset",BindingFlags.Instance|BindingFlags.NonPublic)!;
    static void Require(bool ok,string message) {if(!ok) throw new InvalidDataException(message);}
    static string Hash(byte[] bytes)=>Convert.ToHexString(SHA256.HashData(bytes));
    public static void Text(string inputDirectory,string outputDirectory,bool nativeLantern=false)
    {
        outputDirectory=Path.GetFullPath(outputDirectory);
        Require(outputDirectory.StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\",StringComparison.OrdinalIgnoreCase),"Output outside research workspace");
        var texts=new Dictionary<string,Dictionary<int,string>> {
            ["itemname.fmg"]=new() {{Oil,"Lantern Oil"}},
            ["simpleexplanation.fmg"]=new() {{Oil,"Refill the lantern with five minutes of oil."}},
            ["detailedexplanation.fmg"]=new() {{Oil,"A small flask of clear lamp oil.\n\nRefills the Wayfarer's Lantern, providing\nfive minutes of warm, modest light.\n\nA traveller keeps a little light in reserve."}}
        };
        if(nativeLantern) {
            texts["itemname.fmg"].Add(Lantern,"Wayfarer's Lantern");
            texts["simpleexplanation.fmg"].Add(Lantern,"A reusable lantern. Its fuel reserve is shown by the native torch meter.");
            texts["detailedexplanation.fmg"].Add(Lantern,"A brass lantern carried by a prepared wayfarer.\n\nUse it from the quick slot to draw or stow the lantern.\nLantern Oil replenishes its fuel reserve.");
        }
        var report=new List<object>();
        foreach(var pair in texts)
        {
            string input=Path.Combine(inputDirectory,pair.Key),output=Path.Combine(outputDirectory,pair.Key);
            Require(!File.Exists(output),"FMG output already exists");
            byte[] original=File.ReadAllBytes(input);var f=FMG.Read(original);
            Require(f.Entries.Any(e=>e.ID==Torch),"Expected exact Torch item FMG ID absent");
            Require(f.Entries.All(e=>e.ID!=Oil),"Oil FMG ID already present");
            Require(!nativeLantern||f.Entries.All(e=>e.ID!=Lantern),"Lantern FMG ID already present");
            var before=f.Entries.Select(e=>(e.ID,e.Text)).ToArray();f.Entries.Add(new FMG.Entry(Oil,pair.Value[Oil]));
            if(nativeLantern) foreach(var entry in pair.Value.Where(x=>x.Key==Lantern)) f.Entries.Add(new FMG.Entry(entry.Key,entry.Value));
            f.Entries=f.Entries.OrderBy(e=>e.ID).ToList();
            Directory.CreateDirectory(outputDirectory);f.Write(output);var now=FMG.Read(output);
            Require(now.Entries.Count==before.Length+(nativeLantern?2:1),"FMG count changed unexpectedly");
            foreach(var old in before)Require(now.Entries.Single(e=>e.ID==old.ID).Text==old.Text,"Existing text changed");
            Require(now.Entries.Single(e=>e.ID==Oil).Text==pair.Value[Oil],"Oil text missing");
            if(nativeLantern) Require(now.Entries.Single(e=>e.ID==Lantern).Text==pair.Value[Lantern],"Lantern text missing");
            report.Add(new {File=pair.Key,Input=Path.GetFullPath(input),SourceSHA256=Hash(original),OutputSHA256=Hash(File.ReadAllBytes(output)),PreservedEntries=before.Length,AddedIDs=nativeLantern?new[]{Oil,Lantern}:new[]{Oil}});
        }
        File.WriteAllText(Path.Combine(outputDirectory,"text.audit.json"),JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine("Three active-baseline FMGs extended and re-read; every existing text preserved. Research workspace only.");
    }
    public static void Build(string input,string schemaDirectory,string output,bool testStart=false,bool customIcon=false,bool oilAwardHook=false,bool testPrecreation=false,bool torchFlame=false,bool compactFlameCandidate=false,bool nativeLantern=false)
    {
        output=Path.GetFullPath(output);
        Require(output.StartsWith(@"F:\Modding\Dark Souls 2\research\lantern-20260925\",StringComparison.OrdinalIgnoreCase),"Output outside research workspace");
        Require(!File.Exists(output),"Output already exists; use a new fixture filename");
        byte[] baseline=File.ReadAllBytes(input);
        Require(new[]{"6F9194CC1E6C439CB5502DA17C4AC2F8FFACC5123EE9E6043482179936A3E2E9","619A0FDD266F2677179460204D875C9EF7DD62AF0BA0C70BC2D7511F66E167D3","EB402C6B1F1F638214493386B080F248F14C054318625AE6935FE65A31B254BD","9100BA01D37DE0804CAA89EB8DCE65580BFB5E69F442EB0B5E525B68196E7391"}.Contains(Hash(baseline)),"Unsupported Modern Souls baseline hash");
        var binder=BND4.Read(baseline);
        var before=binder.Files.ToDictionary(e=>e.Name,e=>e.Bytes.ToArray());
        var defs=Directory.GetFiles(schemaDirectory,"*.xml").Select(PARAMDEF.XmlDeserialize).ToDictionary(d=>d.ParamType);
        var coverage=new List<object>();
        var allowedRows=new Dictionary<string,HashSet<int>>();
        var paramsEdited=new Dictionary<string,PARAM>();
        var testFixtureChanges=new List<object>();
        BinderFile Entry(string name)=>binder.Files.Single(f=>Path.GetFileName(f.Name.Replace('\\','/'))==name);
        PARAM Decode(string name) {var p=PARAM.Read(Entry(name).Bytes);p.ApplyParamdef(defs[p.ParamType]);return p;}
        PARAM.Row Clone(PARAM p,int oldId,int newId) {
            Require(p.Rows.All(r=>r.ID!=newId),$"Duplicate ID {newId}");
            var source=p.Rows.Single(r=>r.ID==oldId);var r=new PARAM.Row(newId,source.Name,defs[p.ParamType]);
            for(int i=0;i<source.Cells.Count;i++)r.Cells[i].Value=source.Cells[i].Value is byte[] b?b.ToArray():source.Cells[i].Value;
            p.Rows.Add(r);p.Rows=p.Rows.OrderBy(x=>x.ID).ToList();return r;
        }
        void Save(string name,PARAM p,IEnumerable<int> rows) {Entry(name).Bytes=p.Write();allowedRows[name]=rows.ToHashSet();paramsEdited[name]=p;}

        var item=Decode("ItemParam.param");
        var oil=Clone(item,60310000,Oil);
        oil["specialEffectId"].Value=Oil;
        oil["iconId"].Value=customIcon?Oil:60430000; // The custom icon flag requires an accompanying checked TPF.
        oil["sortId"].Value=301;oil["price"].Value=500;oil["salePrice"].Value=50;
        if(nativeLantern) {
            var lantern=Clone(item,Torch,Lantern);
            lantern["sortId"].Value=302;
            lantern["price"].Value=0;
            lantern["salePrice"].Value=0;
        }
        Save("ItemParam.param",item,Array.Empty<int>());

        if(testStart||testPrecreation) {
            var start=Decode("PlayerStatusParam.param");var rows=new List<int>();
            foreach(int id in new[]{10,100}.Where(id=>id==10?testPrecreation:testStart)) {
                var row=start.Rows.Single(r=>r.ID==id);
                var original=row.Cells.Select(c=>c.Value is byte[] b?b.ToArray():c.Value).ToArray();
                Require(Convert.ToInt32(row["beltItem02"].Value)==-1&&Convert.ToInt32(row["beltItem02Num"].Value)==0,$"Test row{id} quick slot2 already used");
                int lanternSlot=0;
                if(nativeLantern) {
                    lanternSlot=Enumerable.Range(1,10).FirstOrDefault(n=>n!=2&&Convert.ToInt32(row[$"beltItem{n:D2}"].Value)==-1&&Convert.ToInt32(row[$"beltItem{n:D2}Num"].Value)==0);
                    Require(lanternSlot!=0,$"Test row{id} has no empty quick slot for the Lantern");
                }
                Require(Convert.ToInt32(row["genderType"].Value)==0,$"Test row{id} must already use male body");
                int sourceBody=Convert.ToInt32(row["bodyArmor"].Value);
                Require(sourceBody==(id==10?21100101:21030101),$"Unexpected test row{id} body armor");
                row["beltItem02"].Value=Oil;row["beltItem02Num"].Value=(ushort)10;
                if(nativeLantern) {row[$"beltItem{lanternSlot:D2}"].Value=Lantern;row[$"beltItem{lanternSlot:D2}Num"].Value=(ushort)1;}
                if(id==10)row["bodyArmor"].Value=21030101;
                var allowed=new HashSet<string>{"beltItem02","beltItem02Num"};if(nativeLantern){allowed.Add($"beltItem{lanternSlot:D2}");allowed.Add($"beltItem{lanternSlot:D2}Num");}if(id==10)allowed.Add("bodyArmor");
                for(int n=0;n<row.Cells.Count;n++)if(!allowed.Contains(row.Cells[n].Def.InternalName))
                    Require(original[n] is byte[] bytes?bytes.SequenceEqual((byte[])row.Cells[n].Value):Equals(original[n],row.Cells[n].Value),$"Unrelated test row{id} field changed");
                testFixtureChanges.Add(new{Row=id,Role=id==10?"precreation-Default-QA-only":"Swordsman-QA-only",SourceGender=0,SourceBodyArmor=sourceBody,BodyArmor=21030101,LanternSlot=lanternSlot,LanternItem=nativeLantern?Lantern:-1,SourceBeltItem02=-1,SourceBeltItem02Num=0,BeltItem02=Oil,BeltItem02Num=10,OnlyAllowedFields=allowed.OrderBy(x=>x).ToArray(),OtherFieldsVerified=row.Cells.Count-allowed.Count});
                rows.Add(id);
            }
            Save("PlayerStatusParam.param",start,rows);
        }

        var light=Decode("PlayerLight.param");var lamp=Clone(light,17,Light);
        lamp["diffuseA"].Value=(ushort)134;lamp["specularA"].Value=(ushort)134;lamp["radius"].Value=6f;
        if(torchFlame)foreach(string prefix in new[]{"diffuse","specular"}) {
            lamp[prefix+"R"].Value=(byte)255;lamp[prefix+"G"].Value=(byte)158;lamp[prefix+"B"].Value=(byte)64;
        }
        Save("PlayerLight.param",light,Array.Empty<int>());

        var sourceEffects=EMEVD.Read(Entry("SpEffectSpell.emevd").Bytes);
        var sourceEffect=sourceEffects.Events.Single(e=>e.ID==31270000);
        var active=EMEVD.Read(Entry("SpEffectActiveItem.emevd").Bytes);
        Require(active.Events.All(e=>e.ID!=Oil),"Effect ID already present");
        var effect=new EMEVD.Event(Oil);
        var sfx=sourceEffect.Instructions.Single(i=>i.Bank==100040&&i.ID==1);
        var status=sourceEffect.Instructions.Single(i=>i.Bank==1000&&i.ID==9);
        Require(Convert.ToHexString(sfx.ArgData)=="421400000A01000000000000","Cast Light SFX mismatch");
        Require(Convert.ToHexString(status.ArgData)=="130000000000344232000000","Cast Light native-light instruction mismatch");
        var sfxBytes=sfx.ArgData.ToArray();sfxBytes[4]=(byte)(torchFlame?247:30);
        if(compactFlameCandidate&&!torchFlame)throw new Exception("Compact flame candidate requires the shared torch wick anchor.");
        if(torchFlame)BitConverter.GetBytes(compactFlameCandidate?CompactFlame.EffectId:230).CopyTo(sfxBytes,0);
        var statusBytes=status.ArgData.ToArray();BitConverter.GetBytes(300f).CopyTo(statusBytes,4);BitConverter.GetBytes(Light).CopyTo(statusBytes,8);
        effect.Instructions.Add(new EMEVD.Instruction(sfx.Bank,sfx.ID,sfxBytes));
        effect.Instructions.Add(new EMEVD.Instruction(status.Bank,status.ID,statusBytes));
        active.Events.Add(effect);active.Events=active.Events.OrderBy(e=>e.ID).ToList();Entry("SpEffectActiveItem.emevd").Bytes=active.Write();

        foreach(string name in new[]{"ItemLotParam2_Chr.param","ItemLotParam2_Other.param","ItemLotParam2_SvrEvent.param"})
        {
            var p=Decode(name);var edited=new List<int>();
            foreach(var row in p.Rows)
            {
                var torches=Enumerable.Range(1,10).Where(n=>Convert.ToInt32(row[$"itemId{n}"].Value)==60420000&&Convert.ToInt32(row[$"itemNum{n}"].Value)>0).ToArray();
                if(torches.Length==0)continue;
                byte type=Convert.ToByte(row["lotDropType"].Value);
                int count=torches.Sum(n=>Convert.ToInt32(row[$"itemNum{n}"].Value));
                var empty=Enumerable.Range(1,10).FirstOrDefault(n=>Convert.ToInt32(row[$"itemNum{n}"].Value)==0&&Convert.ToSingle(row[$"itemWeight{n}"].Value)==0f);
                bool patch=!oilAwardHook&&type==3&&empty>0&&count<=255;
                coverage.Add(new {Param=name,Row=row.ID,DropType=type,TorchSlots=torches,Count=count,OilSlot=patch?empty:0,Status=oilAwardHook?"runtime-oil-award-hook-required":patch?"fixture-added-guaranteed-companion":"BLOCKED-requires-post-award-hook"});
                if(!patch)continue;
                row[$"itemId{empty}"].Value=(uint)Oil;row[$"itemNum{empty}"].Value=(byte)count;
                row[$"itemWeight{empty}"].Value=1f;
                // Preserve slot category/strength/attribute semantics by copying the known Torch slot.
                foreach(var prefix in new[]{"reinforceLevel","attribute","bExFlag"})row[$"{prefix}{empty}"].Value=row[$"{prefix}{torches[0]}"].Value;
                edited.Add(row.ID);
            }
            if(edited.Count>0)Save(name,p,edited);
        }

        // Verify every original row's raw bytes, allowing only the declared fixed-lot rows to differ.
        int unchangedRows=0;
        foreach(var edit in paramsEdited)
        {
            var oldBytes=before[Entry(edit.Key).Name];var newBytes=Entry(edit.Key).Bytes;
            var old=PARAM.Read(oldBytes);var now=PARAM.Read(newBytes);
            Require(old.DetectedSize==now.DetectedSize,$"Row size changed: {edit.Key}");
            foreach(var row in old.Rows)
            {
                var newRow=now.Rows.Single(r=>r.ID==row.ID);
                if(allowedRows[edit.Key].Contains(row.ID))continue;
                int a=checked((int)(long)Offset.GetValue(row)!),b=checked((int)(long)Offset.GetValue(newRow)!);
                Require(oldBytes.AsSpan(a,(int)old.DetectedSize).SequenceEqual(newBytes.AsSpan(b,(int)now.DetectedSize)),"Unrelated original row changed");unchangedRows++;
            }
        }
        var oldEffects=EMEVD.Read(before[Entry("SpEffectActiveItem.emevd").Name]);
        var newEffects=EMEVD.Read(Entry("SpEffectActiveItem.emevd").Bytes);
        foreach(var e in oldEffects.Events){var n=newEffects.Events.Single(x=>x.ID==e.ID);Require(n.Instructions.Count==e.Instructions.Count,"Original event changed");for(int i=0;i<e.Instructions.Count;i++)Require(n.Instructions[i].Bank==e.Instructions[i].Bank&&n.Instructions[i].ID==e.Instructions[i].ID&&n.Instructions[i].ArgData.SequenceEqual(e.Instructions[i].ArgData),"Original effect instructions changed");}
        var changed=binder.Files.Where(f=>!before[f.Name].SequenceEqual(f.Bytes)).Select(f=>f.Name).ToArray();
        Require(changed.Length==3+(oilAwardHook?0:2)+(testStart||testPrecreation?1:0),"Unexpected changed archive-entry count");
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);binder.Write(output);
        var reloaded=BND4.Read(output);Require(reloaded.Files.Count==binder.Files.Count,"Archive entry count changed");
        for(int i=0;i<binder.Files.Count;i++)Require(reloaded.Files[i].Name==binder.Files[i].Name&&reloaded.Files[i].ID==binder.Files[i].ID&&reloaded.Files[i].Flags==binder.Files[i].Flags&&reloaded.Files[i].Bytes.SequenceEqual(binder.Files[i].Bytes),"Archive round-trip mismatch");
        string role=testPrecreation?(testStart?"test-precreation-and-Swordsman-not-for-shipping":"test-precreation-Default-only-not-for-shipping"):testStart?"test-start-Swordsman-only-not-for-shipping":"content-prototype";
        var report=new {Status="RESEARCH FIXTURE ONLY; requires model, FMGs, icon and live validation. No runtime oil/HUD, native toggle or random-drop pairing; no deployment",Role=role,TestStartOil=testStart?10:0,TestPrecreationOil=testPrecreation?10:0,TestFixtureChanges=testFixtureChanges,OilAwardHookRequired=oilAwardHook,CustomIconRequired=customIcon,BaselineSHA256=Hash(baseline),FixtureSHA256=Hash(File.ReadAllBytes(output)),OilItemID=Oil,LightID=Light,OilSeconds=300,LightIntensity=134,TorchIntensity=382,Radius=6,TorchRadius=16,LightRGB=torchFlame?new[]{255,158,64}:new[]{255,192,128},SfxID=torchFlame?(compactFlameCandidate?CompactFlame.EffectId:230):5186,PelvisSfxDummy=torchFlame?247:30,CustomCharacterAnchorRequired=torchFlame,NativeTorchFlameScaleUnchanged=torchFlame&&!compactFlameCandidate,CompactFlameEmpiricalCandidate=compactFlameCandidate,FlameContainmentLiveVerified=false,WorldLightHipAnchorVerified=false,ChangedEntries=changed,UntouchedEntries=binder.Files.Count-changed.Length,OriginalRowsVerified=unchangedRows,OriginalActiveEffectsVerified=oldEffects.Events.Count,TorchCoverage=coverage};
        File.WriteAllText(output+".audit.json",JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true}));
        Console.WriteLine($"Fixture ready; {coverage.Count} torch lots audited; {unchangedRows} original rows verified; {binder.Files.Count-changed.Length} entries unchanged. No deployment.");
    }
}
