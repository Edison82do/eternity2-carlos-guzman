import java.nio.file.Files;
import java.nio.file.Paths;

import org.alcibiade.eternity.editor.log.NullLog;
import org.alcibiade.eternity.editor.model.GridModel;
import org.alcibiade.eternity.editor.solver.ClusterManager;
import org.alcibiade.eternity.editor.solver.EternitySolver;
import org.alcibiade.eternity.editor.solver.RandomFactory;
import org.alcibiade.eternity.editor.solver.SolverFactory;
import org.alcibiade.eternity.editor.solver.path.PathFactory;

/**
 * Cronometra un solver del Eternity II Editor con precisión de milisegundos
 * (ConsoleApp solo revisa cada 1 s). Uso:
 *   java -cp EternityEditor-1.6.0.jar:. Cronometro modelo.txt "Iterative Path MkIV|Human" semilla segundos
 * Imprime: RESULTADO resuelto=<true|false> puntos=<n>/<max> ms=<t> iteraciones=<i>
 */
public class Cronometro {
    public static void main(String[] a) throws Exception {
        String texto = new String(Files.readAllBytes(Paths.get(a[0])), "UTF-8");
        String[] spec = a[1].split("\\|");
        long semilla = Long.parseLong(a[2]);
        long limiteMs = (long) (Double.parseDouble(a[3]) * 1000);

        RandomFactory.setSeed(semilla);
        GridModel problema = new GridModel();
        GridModel solucion = new GridModel();
        problema.fromQuadString(texto);
        problema.shuffle();

        ClusterManager cm = new ClusterManager(new NullLog());
        EternitySolver s = SolverFactory.createSolver(spec[0], problema, solucion, cm,
                PathFactory.createPath(spec.length > 1 ? spec[1] : "Linear"));
        int n = problema.getSize();
        int max = 2 * n * (n - 1);
        long t0 = System.nanoTime();
        s.start();
        s.join(limiteMs);
        long ms = (System.nanoTime() - t0) / 1_000_000;
        boolean vivo = s.isAlive();
        if (vivo) {
            s.interrupt();
            s.join(2000);
        }
        System.out.println("RESULTADO resuelto=" + cm.isSolutionFound() + " puntos=" + cm.getBestScore()
                + "/" + max + " ms=" + ms + " iteraciones=" + s.getIterations());
        System.exit(0);
    }
}
