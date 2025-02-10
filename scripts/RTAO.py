from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('BilateralBlur', 'BilateralBlur', {'numIterations': 1, 'kernelSize': 2, 'betterSlope': True})
    g.create_pass('DeferredLighting', 'DeferredLighting', {'ambientLight': 0.0, 'aoBlendMode': 3})
    g.create_pass('GBufferLite', 'GBufferLite', {'jitterSamplePattern': 'Center', 'jitterSamplesCount': 16})
    g.create_pass('RTAO', 'RTAO', {})
    g.add_edge('BilateralBlur.colorOut', 'DeferredLighting.ambientOcclusion')
    g.add_edge('GBufferLite.posW', 'DeferredLighting.posW')
    g.add_edge('GBufferLite.normW', 'DeferredLighting.normW')
    g.add_edge('GBufferLite.diffuseOpacity', 'DeferredLighting.diffuseOpacity')
    g.add_edge('GBufferLite.specRough', 'DeferredLighting.specRough')
    g.add_edge('GBufferLite.posW', 'RTAO.wPos')
    g.add_edge('RTAO.ambient', 'BilateralBlur.colorIn')
    g.add_edge('GBufferLite.normW', 'RTAO.faceNormal')
    g.add_edge('GBufferLite.linearDepth', 'BilateralBlur.linearDepthIn')
    g.mark_output('DeferredLighting.kColorOut')
    g.mark_output('BilateralBlur.colorOut')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
