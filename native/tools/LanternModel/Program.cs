// Local asset builder. Proprietary inputs/outputs stay outside distributable source.
using SoulsFormats;
using System.Numerics;
using System.Security.Cryptography;
using System.Text.Json;

System.Text.Encoding.RegisterProvider(System.Text.CodePagesEncodingProvider.Instance);
if(args.Length==5 && args[0]=="baked-refresh") { BakedRefresh.Run(args[1],args[2],args[3],args[4]); return; }
if(args.Length==3 && args[0]=="right-rear-hip") { RightHip.Run(args[1],args[2]); return; }
if(args.Length==2 && args[0]=="inspect-wick") { WickInspection.Run(args[1]); return; }
if(args.Length==3 && args[0]=="flame-anchor") { FlameAnchor.Build(args[1],args[2]); return; }
if(args.Length==4 && args[0]=="flame-anchor") { FlameAnchor.Build(args[1],args[3],args[2]); return; }
if(args.Length==4 && args[0]=="build") { ModelBuild.Run(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="pivot-candidate") { PivotBuild.Run(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="base-lamp-candidate") { BaseLampBuild.Run(args[1],args[2],args[3]); return; }
if(args.Length==4 && args[0]=="refresh-base") { BaseRefresh.Run(args[1],args[2],args[3]); return; }
if(args.Length != 2 || args[0] != "inspect") throw new Exception("Usage: LanternModel inspect <body.bnd>");
var binder=BND4.Read(args[1]);
var entry=binder.Files.First(f=>FLVER2.Is(f.Bytes));
var model=FLVER2.Read(entry.Bytes);
var report=new {
    sha256=Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[1]))),
    entry=entry.Name,version=model.Header.Version,
    bounds=new{min=model.Header.BoundingBoxMin,max=model.Header.BoundingBoxMax},
    bones=model.Bones.Select((b,i)=>new{i,b.Name,b.ParentIndex,b.Translation,b.Rotation,b.Scale,b.BoundingBoxMin,b.BoundingBoxMax}),
    materials=model.Materials.Select((m,i)=>new{i,m.Name,m.MTD,m.Flags,m.GXIndex,textures=m.Textures}),
    layouts=model.BufferLayouts,
    meshes=model.Meshes.Select((m,i)=>new{i,m.Dynamic,m.MaterialIndex,m.DefaultBoneIndex,m.BoneIndices,layouts=m.VertexBuffers.Select(b=>b.LayoutIndex),vertices=m.Vertices.Count,faces=m.FaceSets.Select(f=>new{f.Flags,f.TriangleStrip,f.Unk06,f.CullBackfaces,count=f.Indices.Count}),sample=m.Vertices.Take(2)})
};
Console.WriteLine(JsonSerializer.Serialize(report,new JsonSerializerOptions{WriteIndented=true,IncludeFields=true}));
