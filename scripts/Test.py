from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_Test():
    g = RenderGraph('Test')
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('StochasticDepthStratfield', 'StochasticDepthStratfield', {'alpha': 0.10000000149011612, 'numSamples': 8})
    g.add_edge('GBufferRaster.depth', 'StochasticDepthStratfield.depthTexture')
    g.add_edge('GBufferRaster', 'StochasticDepthStratfield')
    g.mark_output('StochasticDepthStratfield.stochasticDepth')
    return g

Test = render_graph_Test()
try: m.addGraph(Test)
except NameError: None
