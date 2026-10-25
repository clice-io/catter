// Built for any CPU: a 32-bit executable that Windows runs as a 64-bit
// process.
class App {
    static int Main() {
        System.Console.WriteLine("64-bit: " + System.Environment.Is64BitProcess);
        return 7;
    }
}
