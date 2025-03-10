from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('VAO', 'VAO', {'kVaoRadius': 0.5, 'kVaoExponent': 2.0, 'kSampleCount': 8, 'resolutionDivisor': 1, 'enableGuardBand': True, 'SVAOInputMode': True, 'useRayInterval': True})
    g.create_pass('LinearizeDepth', 'LinearizeDepth', {})
    g.create_pass('NormalsToViewSpace', 'NormalsToViewSpace', {})
    g.create_pass('RTStochasticDepth', 'RTStochasticDepth', {'resolutionDivisor': 1, 'enableGuardBand': True})
    g.create_pass('SVAO', 'SVAO', {'kVaoRadius': 0.5, 'kVaoExponent': 2.0, 'kSampleCount': 8, 'resolutionDivisor': 1, 'enableGuardBand': True})
    g.create_pass('BilateralBlur', 'BilateralBlur', {'numIterations': 1, 'kernelSize': 2, 'betterSlope': True})
    g.add_edge('LinearizeDepth.linearDepthOut', 'VAO.linearDepthIn')
    g.add_edge('GBufferRaster.depth', 'LinearizeDepth.depthIn')
    g.add_edge('GBufferRaster.normW', 'NormalsToViewSpace.normalsWorldIn')
    g.add_edge('NormalsToViewSpace.normalsViewOut', 'VAO.normalViewIn')
    g.add_edge('LinearizeDepth.linearDepthOut', 'SVAO.linearDepthIn')
    g.add_edge('LinearizeDepth.linearDepthOut', 'RTStochasticDepth.linearDepthIn')
    g.add_edge('RTStochasticDepth.stochasticDepth', 'SVAO.stochDepthIn')
    g.add_edge('VAO.aoOut', 'SVAO.aoInOut')
    g.add_edge('VAO.aoMaskOut', 'SVAO.aoMaskIn')
    g.add_edge('NormalsToViewSpace.normalsViewOut', 'SVAO.normalViewIn')
    g.add_edge('SVAO.aoInOut', 'BilateralBlur.colorIn')
    g.add_edge('LinearizeDepth.linearDepthOut', 'BilateralBlur.linearDepthIn')
    g.add_edge('VAO.rayMinOut', 'RTStochasticDepth.rayMinIn')
    g.add_edge('VAO.rayMaxOut', 'RTStochasticDepth.rayMaxIn')
    g.add_edge('VAO', 'RTStochasticDepth')
    g.add_edge('RTStochasticDepth', 'SVAO')
    g.mark_output('BilateralBlur.colorOut')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
