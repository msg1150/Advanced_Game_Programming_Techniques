"""대형 데모 Terrain의 오프라인 생성기. runtime에서는 이 파일을 호출하지 않는다.
Python 3 + numpy. 1024x1024 Cell -> 256 independent binary Tiles + Metadata.
"""
from pathlib import Path
import struct
import numpy as np
N=1024; CELLS=64; SIZE=.25; ORIGIN=-N*SIZE/2
out=Path(__file__).resolve().parents[1]/'Assets'/'TerrainTiles'
out.mkdir(parents=True,exist_ok=True)
a=np.arange(N+1,dtype=np.float32)*np.float32(SIZE)+np.float32(ORIGIN)
X,Z=np.meshgrid(a,a)
h=(.35+1*np.sin(X*.035)*np.cos(Z*.028)
   +5.8*np.exp(-(((X+42)/39)**2+((Z-15)/46)**2))*(.80+.20*np.sin(X*.11))
   +9.5*np.exp(-(((X-28)/37)**2+((Z+35)/30)**2))
   +12*np.exp(-(((X-45)/23)**2+((Z-48)/26)**2))
   +3.3*np.exp(-(((X+70)/29)**2+((Z+66)/25)**2))
   +.7*np.sin(X*.15+np.cos(Z*.06))*np.sin(Z*.17)
   +.25*np.cos(X*.56)*np.sin(Z*.41)).astype('<f4')
with (out/'Terrain.meta').open('wb') as f:
    f.write(struct.pack('<4s6I3f',b'DTM1',1,N,N,CELLS,N//CELLS,N//CELLS,SIZE,ORIGIN,ORIGIN))
    for z in range(N//CELLS):
        for x in range(N//CELLS):
            inside=h[z*CELLS:z*CELLS+CELLS+1,x*CELLS:x*CELLS+CELLS+1]
            f.write(struct.pack('<2f',float(inside.min()),float(inside.max())))
for z in range(N//CELLS):
    for x in range(N//CELLS):
        rows=np.clip(np.arange(z*CELLS-1,z*CELLS+CELLS+2),0,N)
        cols=np.clip(np.arange(x*CELLS-1,x*CELLS+CELLS+2),0,N)
        samples=h[np.ix_(rows,cols)].astype('<f4')
        with (out/f'Tile_{x:02}_{z:02}.tile').open('wb') as f:
            f.write(struct.pack('<4s5I',b'DTL1',1,x,z,CELLS,CELLS))
            f.write(samples.tobytes())
print('Generated',len(list(out.glob('*.tile'))),'tiles in',out)
