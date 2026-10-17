using SoulsFormats;
using System.Text.Json;
using System.Security.Cryptography;
System.Text.Encoding.RegisterProvider(System.Text.CodePagesEncodingProvider.Instance);
if(args.Length==4&&args[0]=="lantern-flame-audio"){LanternFlameAudio.Build(args[1],args[2],float.Parse(args[3],System.Globalization.CultureInfo.InvariantCulture));return;}
if(args.Length==7&&args[0]=="split-lantern-pack"){SplitLanternPack.Build(args[1],args[2],args[3],args[4],args[5],args[6]);return;}
if(args.Length==7&&args[0]=="lantern-bow-animation"){LanternBowAnimation.Build(args[1],args[2],args[3],args[4],args[5],args[6]);return;}
if(args.Length==4&&args[0]=="lantern-animation-rate"){LanternAnimationRate.Build(args[1],args[2],args[3]);return;}
if(args.Length==4&&args[0]=="lantern-bow-slow-rate"){LanternBowSlowRate.Build(args[1],args[2],args[3]);return;}
if(args.Length==4&&args[0]=="lantern-bonfire-delivery"){LanternBonfireDelivery.Build(args[1],args[2],args[3]);return;}
if(args.Length==4&&args[0]=="remove-lantern-use-effect"){LanternUseEffectRemoval.Build(args[1],args[2],args[3]);return;}
if(args.Length==5&&args[0]=="lantern-animation-candidate"){LanternAnimation.Build(args[1],args[2],args[3],args[4]);return;}
if(args.Length==3&&args[0]=="extract-lantern-icon"){LanternPolish.Extract(args[1],args[2]);return;}
if(args.Length==5&&args[0]=="polish-lantern"){LanternPolish.Build(args[1],args[2],args[3],args[4]);return;}
if(args.Length==3&&args[0]=="simple-lantern"){SimpleLantern.Build(args[1],args[2]);return;}
if(args.Length==3&&args[0]=="lantern-presentation"){LanternPresentation.Build(args[1],args[2]);return;}
if(args.Length==4&&args[0]=="retire-oil-variants"){RetireOilVariants.Build(args[1],args[2],args[3]);return;}
if(args.Length==3 && args[0]=="compact-flame-candidate") {CompactFlame.Build(args[1],args[2]);return;}
if(args.Length==3 && args[0]=="centered-flame-candidate") {CompactFlame.Build(args[1],args[2],true);return;}
if(args.Length>=4 && args[0]=="build") {
    if(args.Skip(4).Any(x=>x!="--test-start"&&x!="--test-precreation"&&x!="--custom-icon"&&x!="--oil-award-hook"&&x!="--torch-flame"&&x!="--compact-flame-candidate"&&x!="--native-lantern"))throw new ArgumentException("Unknown build flag");
    LanternFixture.Build(args[1],args[2],args[3],args.Contains("--test-start"),args.Contains("--custom-icon"),args.Contains("--oil-award-hook"),args.Contains("--test-precreation"),args.Contains("--torch-flame"),args.Contains("--compact-flame-candidate"),args.Contains("--native-lantern"));return;
}
if(args.Length>=4 && args[0]=="native-lantern") {
    if(args.Skip(4).Any(x=>x!="--test-start"&&x!="--test-precreation"))throw new ArgumentException("Unknown native-lantern flag");
    NativeLantern.Build(args[1],args[2],args[3],args.Contains("--test-start"),args.Contains("--test-precreation"));return;
}
if(args.Length==4 && args[0]=="repairable-lantern") { RepairableLantern.Build(args[1],args[2],args[3]); return; }
if(args.Length==3 && args[0]=="repairable-lantern-text") { RepairableLantern.Text(args[1],args[2]); return; }
if(args.Length==4 && args[0]=="things-betwixt-pickup") { ThingsBetwixtPickup.Build(args[1],args[2],args[3]); return; }
if(args.Length==3 && args[0]=="things-betwixt-pickup-text") { ThingsBetwixtPickup.Text(args[1],args[2]); return; }
if(args.Length==4 && args[0]=="fix-inventory-lantern") { InventoryLanternFix.Build(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="fix-reusable-lantern") { ReusableLanternFix.Build(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="restore-reusable-lantern-toggle") { RestoreReusableLanternToggle.Build(args[1],args[2],args[3]); return; }
if(args.Length==7 && args[0]=="repair-reusable-lantern-variants") { RepairReusableLanternVariants.Build(args[1],args[2],args[3],args[4],args[5],args[6]); return; }
if(args.Length==4 && args[0]=="quick-slot-lantern-toggle") { QuickSlotLanternToggle.Build(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="remove-legacy-oil-light") { RemoveLegacyOilLight.Build(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="lantern-fuel-and-animation") { LanternFuelAndAnimation.Build(args[1],args[2],args[3]); return; }
if(args.Length==3 && args[0]=="native-lantern-text") {NativeLantern.Text(args[1],args[2]);return;}
if((args.Length==3||args.Length==4) && args[0]=="text") {if(args.Length==4&&args[3]!="--native-lantern")throw new ArgumentException("Unknown text flag");LanternFixture.Text(args[1],args[2],args.Contains("--native-lantern"));return;}
if(args.Length==4 && args[0]=="icon") {
    var reference=System.IO.File.ReadAllBytes(args[1]);
    var referenceHash=Convert.ToHexString(SHA256.HashData(reference));
    if(referenceHash!="86793C344A55F2BE7FF3D21790A14EDBAFBD0984F69C94665E3E6B86F5EBB275" && referenceHash!="E764AC1E70A5D264A2EAF4A46BA52A66273BE214AF42E2FE55189933E4AFE288")throw new Exception("Unknown icon reference");
    var dds=System.IO.File.ReadAllBytes(args[2]);
    if(dds.Length!=8320||System.Text.Encoding.ASCII.GetString(dds,0,4)!="DDS "||BitConverter.ToInt32(dds,12)!=128||BitConverter.ToInt32(dds,16)!=64||System.Text.Encoding.ASCII.GetString(dds,84,4)!="DXT5")throw new Exception("Expected 64x128 one-mip BC3 DDS with legacy DXT5 header");
    if(System.IO.File.Exists(args[3]))throw new Exception("Icon output exists");
    var t=TPF.Read(reference);t.Textures[0].Name="IT_IC_0060425000";t.Textures[0].Bytes=dds;
    t.Write(args[3]);var verified=TPF.Read(args[3]);
    if(verified.Textures.Count!=1||verified.Textures[0].Name!="IT_IC_0060425000"||verified.Textures[0].Format!=5||!verified.Textures[0].Bytes.SequenceEqual(dds))throw new Exception("Icon TPF round-trip mismatch");
    Console.WriteLine("Custom oil icon packed and verified: 64x128 BC3, native metadata preserved.");return;
}
if(args.Length==5 && args[0]=="icon-named") {
    var itemId=int.Parse(args[1]);
    if(itemId<1||itemId>99999999)throw new ArgumentOutOfRangeException(nameof(itemId));
    var reference=System.IO.File.ReadAllBytes(args[2]);
    var referenceHash=Convert.ToHexString(SHA256.HashData(reference));
    if(referenceHash!="86793C344A55F2BE7FF3D21790A14EDBAFBD0984F69C94665E3E6B86F5EBB275" && referenceHash!="E764AC1E70A5D264A2EAF4A46BA52A66273BE214AF42E2FE55189933E4AFE288")throw new Exception("Unknown icon reference");
    var dds=System.IO.File.ReadAllBytes(args[3]);
    if(dds.Length!=8320||System.Text.Encoding.ASCII.GetString(dds,0,4)!="DDS "||BitConverter.ToInt32(dds,12)!=128||BitConverter.ToInt32(dds,16)!=64||System.Text.Encoding.ASCII.GetString(dds,84,4)!="DXT5")throw new Exception("Expected 64x128 one-mip BC3 DDS with legacy DXT5 header");
    if(System.IO.File.Exists(args[4]))throw new Exception("Icon output exists");
    var t=TPF.Read(reference);t.Textures[0].Name=$"IT_IC_{itemId:D10}";t.Textures[0].Bytes=dds;t.Write(args[4]);var verified=TPF.Read(args[4]);
    if(verified.Textures.Count!=1||verified.Textures[0].Name!=$"IT_IC_{itemId:D10}"||verified.Textures[0].Format!=5||!verified.Textures[0].Bytes.SequenceEqual(dds))throw new Exception("Icon TPF round-trip mismatch");
    Console.WriteLine($"Custom item icon {itemId} packed and verified: 64x128 BC3, native metadata preserved.");return;
}
if(args.Length==2 && args[0]=="inspect-tpf") {
    var t=TPF.Read(args[1]);
    Console.WriteLine(JsonSerializer.Serialize(new{t.Platform,t.Encoding,t.Flag2,Textures=t.Textures.Select(x=>new{x.Name,x.Format,x.Type,x.Mipmaps,x.Flags1,Bytes=x.Bytes.Length,HeaderHex=Convert.ToHexString(x.Bytes.Take(128).ToArray())})},new JsonSerializerOptions{WriteIndented=true}));return;
}
if(args.Length==3 && args[0]=="inspect-shop") { ShopOil.Inspect(args[1],args[2]); return; }
if(args.Length==4 && args[0]=="add-oil-shops") { ShopOil.Build(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="inspect-item") { ItemInspector.Print(args[1],args[2],int.Parse(args[3])); return; }
if(args.Length==3 && args[0]=="inspect-active-effect") { ItemInspector.PrintActiveEffect(args[1],int.Parse(args[2])); return; }
throw new ArgumentException("LanternContent build ... | things-betwixt-pickup <current Lab regulation> <schema directory> <new output> | things-betwixt-pickup-text <Lab FMG directory> <new output directory> | fix-inventory-lantern <current Lab regulation> <schema directory> <new output> | repairable-lantern <current Lab regulation> <schema directory> <new output> | icon <checked reference TPF> <64x128 BC3 DDS> <new TPF> | inspect-tpf <file>");


