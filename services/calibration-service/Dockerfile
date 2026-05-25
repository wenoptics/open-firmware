FROM python:3.13-slim

WORKDIR /app

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv

# Copy dependency manifests first for layer caching
COPY pyproject.toml uv.lock* ./

# Install production dependencies (no dev group)
RUN uv sync --no-dev --frozen

# Copy application code
COPY main.py geometry.py autocal.GCODE ./

EXPOSE 8000

CMD ["uv", "run", "uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
