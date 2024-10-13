from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_g():
    g = RenderGraph('g')
    g.create_pass('SideBySidePass', 'SideBySidePass', {'splitLocation': -1.0, 'showTextLabels': False, 'leftLabel': 'Left side', 'rightLabel': 'Right side'})
    g.create_pass('LinearizeDepth0', 'LinearizeDepth', {})
    g.create_pass('GBufferRaster0', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('StochasticDepthNaive', 'StochasticDepthNaive', {})
    g.create_pass('LinearizeDepth', 'LinearizeDepth', {})
    g.add_edge('GBufferRaster0.depth', 'LinearizeDepth0.depthIn')
    g.add_edge('GBufferRaster0.depth', 'StochasticDepthNaive.depthTexture')
    g.add_edge('StochasticDepthNaive.stochasticDepth', 'LinearizeDepth.depthIn')
    g.add_edge('LinearizeDepth0.linearDepthOut', 'SideBySidePass.leftInput')
    g.add_edge('LinearizeDepth.linearDepthOut', 'SideBySidePass.rightInput')
    g.add_edge('GBufferRaster0', 'StochasticDepthNaive')
    g.mark_output('SideBySidePass.output')
    return g

g = render_graph_g()
try: m.addGraph(g)
except NameError: None
