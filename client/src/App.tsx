/**
 * 名字竞技场只保留一个公开页面；战斗数值由 C++/WASM 返回，React 仅负责展示。
 */
import Home from "./pages/Home";

export default function App()
{
  return <Home />;
}
