from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('VAO', 'VAO', {'kVaoRadius': 0.5, 'kVaoExponent': 2.0, 'kSampleCount': 16, 'resolutionDivisor': 4, 'enableGuardBand': False, 'useDitherTexture': True, 'SVAOInputMode': False, 'useRayInterval': False, 'usePrepass': False, 'prePassSamplingMode': 'Careful'})
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})
    g.create_pass('NormalsToViewSpace', 'NormalsToViewSpace', {})
    g.create_pass('BilateralBlur', 'BilateralBlur', {'numIterations': 1, 'kernelSize': 4, 'betterSlope': True})
    g.create_pass('GBufferLite', 'GBufferLite', {'jitterSamplePattern': 'DirectX', 'jitterSamplesCount': 16})
    g.create_pass('DeferredLighting', 'DeferredLighting', {'ambientLight': 0.0, 'aoBlendMode': 3})
    g.add_edge('DeferredLighting.kColorOut', 'TAA.colorIn')
    g.add_edge('NormalsToViewSpace.normalsViewOut', 'VAO.normalViewIn')
    g.add_edge('VAO.aoOut', 'BilateralBlur.colorIn')
    g.add_edge('GBufferLite.normW', 'NormalsToViewSpace.normalsWorldIn')
    g.add_edge('GBufferLite.linearDepth', 'VAO.linearDepthIn')
    g.add_edge('BilateralBlur.colorOut', 'DeferredLighting.ambientOcclusion')
    g.add_edge('GBufferLite.specRough', 'DeferredLighting.specRough')
    g.add_edge('GBufferLite.diffuseOpacity', 'DeferredLighting.diffuseOpacity')
    g.add_edge('GBufferLite.normW', 'DeferredLighting.normW')
    g.add_edge('GBufferLite.posW', 'DeferredLighting.posW')
    g.add_edge('GBufferLite.motionVector', 'TAA.motionVecs')
    g.add_edge('GBufferLite.linearDepth', 'BilateralBlur.linearDepthIn')
    g.mark_output('BilateralBlur.colorOut')
    g.mark_output('TAA.colorOut')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
