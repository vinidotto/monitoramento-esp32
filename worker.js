export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    try {

      // ==============================
      // POST /movimento
      // ==============================

      if (
        request.method === "POST" &&
        url.pathname === "/movimento"
      ) {

        const timestamp = Date.now();

        await env.DB
          .prepare(
            "INSERT INTO movimentos (ts) VALUES (?)"
          )
          .bind(timestamp)
          .run();

        return json({
          sucesso: true,
          mensagem: "Movimento registrado",
          ts: timestamp
        });
      }


      // ==============================
      // POST /luz
      // ==============================

      if (
        request.method === "POST" &&
        url.pathname === "/luz"
      ) {

        const body = await request.json();

        const valor = body.valor;
        const acesa = body.acesa ? 1 : 0;

        if (valor === undefined) {
          return json(
            {
              erro: "Campo valor é obrigatório"
            },
            400
          );
        }

        const timestamp = Date.now();

        await env.DB
          .prepare(
            `
            INSERT INTO luz
            (valor, acesa, ts)
            VALUES (?, ?, ?)
            `
          )
          .bind(valor, acesa, timestamp)
          .run();

        return json({
          sucesso: true,
          mensagem: "Leitura de luz registrada",
          valor,
          acesa,
          ts: timestamp
        });
      }


      // ==============================
      // GET /ultimo-movimento
      // ==============================

      if (
        request.method === "GET" &&
        url.pathname === "/ultimo-movimento"
      ) {

        const resultado = await env.DB
          .prepare(
            `
            SELECT *
            FROM movimentos
            ORDER BY ts DESC
            LIMIT 1
            `
          )
          .first();

        return json(resultado);
      }


      // ==============================
      // GET /por-hora
      // ==============================

      if (
        request.method === "GET" &&
        url.pathname === "/por-hora"
      ) {

        const resultado = await env.DB
          .prepare(
            `
            SELECT
              strftime(
                '%Y-%m-%d %H:00',
                ts / 1000,
                'unixepoch'
              ) AS hora,
              COUNT(*) AS movimentos
            FROM movimentos
            GROUP BY hora
            ORDER BY hora DESC
            `
          )
          .all();

        return json(resultado.results);
      }


      // ==============================
      // GET /luz-resumo
      // ==============================

      if (
        request.method === "GET" &&
        url.pathname === "/luz-resumo"
      ) {

        const resultado = await env.DB
          .prepare(
            `
            SELECT
              COUNT(*) AS total,
              AVG(valor) AS media,
              MIN(valor) AS minimo,
              MAX(valor) AS maximo,
              SUM(
                CASE
                  WHEN acesa = 1 THEN 1
                  ELSE 0
                END
              ) AS leituras_luz_acesa
            FROM luz
            `
          )
          .first();

        return json(resultado);
      }


      // ==============================
      // GET /desperdicio
      // ==============================

      if (
        request.method === "GET" &&
        url.pathname === "/desperdicio"
      ) {

        const resultado = await env.DB
          .prepare(
            `
            SELECT
              l.id,
              l.valor,
              l.acesa,
              l.ts
            FROM luz l
            WHERE l.acesa = 1

            AND NOT EXISTS (
              SELECT 1
              FROM movimentos m
              WHERE m.ts
              BETWEEN l.ts - 120000
              AND l.ts
            )

            ORDER BY l.ts DESC
            `
          )
          .all();

        return json(resultado.results);
      }


      // ==============================
      // ROTA PRINCIPAL
      // ==============================

      if (
        request.method === "GET" &&
        url.pathname === "/"
      ) {

        return json({
          projeto: "Monitoramento de sala",
          status: "API funcionando",
          endpoints: [
            "POST /movimento",
            "POST /luz",
            "GET /ultimo-movimento",
            "GET /por-hora",
            "GET /luz-resumo",
            "GET /desperdicio"
          ]
        });
      }


      return json(
        {
          erro: "Rota não encontrada"
        },
        404
      );

    } catch (erro) {

      return json(
        {
          erro: erro.message
        },
        500
      );
    }
  }
};


function json(data, status = 200) {
  return new Response(
    JSON.stringify(data, null, 2),
    {
      status,
      headers: {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*"
      }
    }
  );
}