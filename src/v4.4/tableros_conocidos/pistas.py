"""Herramientas para la investigación de la pieza fija (idea de Carlos).
- resolver un tablero y guardar su solución;
- girar la solución (para que la pieza dada no favorezca la esquina de siempre);
- calcular el argumento -P F,C,K,G de cualquier casilla de esa solución."""
import subprocess, sys, random
def leer(r):
    v=[]
    for l in open(r):
        if l.startswith('#'): continue
        v+=[int(x) for x in l.split()]
    n=int(round((len(v)//4)**.5)); return n,[tuple(v[4*i:4*i+4]) for i in range(len(v)//4)]
def rot(p,k):
    for _ in range(k%4): p=(p[3],p[0],p[1],p[2])
    return p
def girar_tablero(G,veces):
    n=len(G)
    for _ in range(veces): G=[[rot(G[n-1-c][r],1) for c in range(n)] for r in range(n)]
    return G
def pista(pz,G,r,c):
    q=G[r][c]
    k,g=next((k+1,g) for k,p in enumerate(pz) for g in range(4) if rot(p,g)==q)
    return f'{r+1},{c+1},{k},{g}'
def solucion(exe,tab,hilos=2,limite=300):
    out=subprocess.run([exe,tab,'-q','-t',str(hilos),'-l',str(limite),'-o','/tmp/claude-0/sol_p.txt'],capture_output=True,text=True).stdout
    if 'resuelto=1' not in out: return None
    n,s=leer('/tmp/claude-0/sol_p.txt'); return [[s[r*n+c] for c in range(n)] for r in range(n)]
def posiciones(n):
    """centro (como la 139 del oficial: una de las casillas centrales) y las 4 de pista cerca de las esquinas del interior
    (en el 16x16 oficial: C3, C14, N3, N14 -> fila/col 3 y n-2)"""
    centro=(n//2, (n-1)//2)
    esquinas_int=[(2,2),(2,n-3),(n-3,2),(n-3,n-3)]
    return centro, esquinas_int
