#!/bin/bash

# set -euo pipefail
IFS=$'\n\t'

RESULTS_DIR="diff_results"

# ==============================================================================
# 検索設定: 対象とするコミットハッシュの一覧を取得するコマンド
# ==============================================================================
# 例: 特定のユーザーが、特定のディレクトリで行った変更を古い順(--reverse)に取得
# ※ --format="%H" をつけることで、余計な情報を省いてハッシュ値のみを抽出します
TARGET_COMMITS=$(git log --format="%H" --reverse --author="iwancof" -- ".")

# 【参考】他の取得パターンの例：
# 特定の2つのブランチ間の差分コミットを取得する場合
# TARGET_COMMITS=$(git log --format="%H" --reverse main..feature-branch)
# 直近5件のコミットを取得する場合
# TARGET_COMMITS=$(git log --format="%H" --reverse HEAD~5..HEAD)
# ==============================================================================

# コミットが見つからなかった場合の処理
if [ -z "$TARGET_COMMITS" ]; then
    echo "対象となるコミットが見つかりませんでした。検索条件を確認してください。"
    exit 1
fi

# Output directory
if [ ! -d "$RESULTS_DIR" ]; then
    mkdir "$RESULTS_DIR"
else
    rm -rd "${RESULTS_DIR}"/*
fi

# 一時作業用のディレクトリを作成（スクリプト終了時に自動削除されます）
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# 取得したコミットごとにループ処理
for COMMIT in $TARGET_COMMITS; do
    echo "=================================================="
    echo "Commit: $COMMIT"
    echo "=================================================="

    # そのコミットで変更されたC++関連ファイル（.cpp, .c, .h, .hpp）のリストを取得
    FILES=$(git show --name-only --format="" "$COMMIT" | grep -E '\.(cpp|c|h|hpp)$')

    if [ -z "$FILES" ]; then
        echo "C++ファイルの変更はありませんでした。"
        continue
    fi

    for FILE in $FILES; do
        echo "--- File: $FILE ---"
        result_commit_dir="${RESULTS_DIR}/${COMMIT}"
        if [ ! -d "$result_commit_dir" ]; then
            mkdir "$result_commit_dir"
        fi

        # 変更前（^）と変更後（現在のコミット）のファイルの中身を一時ファイルに抽出
        # ※新規作成・削除されたファイルの場合はエラーが出るので /dev/null に捨てて空ファイル扱いにする
        git show "$COMMIT^:$FILE" > "$TMP_DIR/old_file.cpp" 2>/dev/null || touch "$TMP_DIR/old_file.cpp"
        git show "$COMMIT:$FILE" > "$TMP_DIR/new_file.cpp" 2>/dev/null || touch "$TMP_DIR/new_file.cpp"

        # 両方にclang-formatを適用 (スタイルはプロジェクトに合わせて適宜変更してください)
        clang-format -i "$TMP_DIR/old_file.cpp"
        clang-format -i "$TMP_DIR/new_file.cpp"

        # フォーマット後のファイル同士で、見やすい形式(-u)で差分を取る
        diff -u "$TMP_DIR/old_file.cpp" "$TMP_DIR/new_file.cpp" > "${result_commit_dir}/$(basename ${FILE})"
        
        echo ""
    done
done

echo "done."