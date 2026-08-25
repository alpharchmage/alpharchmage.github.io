declare module "*.mjs" {
  type EmscriptenModule = {
    cwrap: (name: string, returnType: string, argumentTypes: string[]) => (input: string) => number;
    UTF8ToString: (pointer: number) => string;
  };

  const createModule: (options: { locateFile: (path: string) => string }) => Promise<EmscriptenModule>;
  export default createModule;
}
