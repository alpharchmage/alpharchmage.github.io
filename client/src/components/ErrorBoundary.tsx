import { Component, ReactNode } from "react";

interface Props {
  children: ReactNode;
}

interface State {
  hasError: boolean;
  error: Error | null;
}

class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = { hasError: false, error: null };
  }

  static getDerivedStateFromError(error: Error): State {
    return { hasError: true, error };
  }

  render() {
    if (this.state.hasError) {
      return (
        <div style={{ minHeight: "100dvh", display: "grid", placeItems: "center", padding: 24, color: "#23434b", background: "#fff" }}>
          <section style={{ width: "min(560px, 100%)", border: "1px solid #b6e3ec", padding: 24 }}>
            <h2>页面发生错误</h2>
            <pre style={{ overflow: "auto", whiteSpace: "pre-wrap" }}>{this.state.error?.stack}</pre>
            <button type="button" onClick={() => window.location.reload()}>重新加载</button>
          </section>
        </div>
      );
    }

    return this.props.children;
  }
}

export default ErrorBoundary;
