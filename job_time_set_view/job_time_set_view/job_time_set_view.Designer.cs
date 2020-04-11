namespace job_time_set_view
{
    partial class job_time_set_view
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.DataSource = new System.Windows.Forms.ListBox();
            this.label1 = new System.Windows.Forms.Label();
            this.ItemProperties = new System.Windows.Forms.ListBox();
            this.label2 = new System.Windows.Forms.Label();
            this.tabControl1 = new System.Windows.Forms.TabControl();
            this.SuspendLayout();
            // 
            // DataSource
            // 
            this.DataSource.FormattingEnabled = true;
            this.DataSource.Location = new System.Drawing.Point(12, 34);
            this.DataSource.Name = "DataSource";
            this.DataSource.Size = new System.Drawing.Size(239, 212);
            this.DataSource.TabIndex = 1;
            this.DataSource.SelectedIndexChanged += new System.EventHandler(this.listBox1_SelectedIndexChanged);
            // 
            // label1
            // 
            this.label1.AccessibleDescription = "DataSourceLabel";
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(98, 18);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(67, 13);
            this.label1.TabIndex = 2;
            this.label1.Text = "Data Source";
            // 
            // ItemProperties
            // 
            this.ItemProperties.AccessibleDescription = "Item properties";
            this.ItemProperties.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.ItemProperties.FormattingEnabled = true;
            this.ItemProperties.Location = new System.Drawing.Point(12, 270);
            this.ItemProperties.Name = "ItemProperties";
            this.ItemProperties.Size = new System.Drawing.Size(239, 420);
            this.ItemProperties.TabIndex = 3;
            // 
            // label2
            // 
            this.label2.AccessibleDescription = "ItemPropertyLabel";
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(93, 254);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(77, 13);
            this.label2.TabIndex = 4;
            this.label2.Text = "Item Properties";
            // 
            // tabControl1
            // 
            this.tabControl1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.tabControl1.Location = new System.Drawing.Point(260, 12);
            this.tabControl1.Name = "tabControl1";
            this.tabControl1.SelectedIndex = 0;
            this.tabControl1.Size = new System.Drawing.Size(739, 695);
            this.tabControl1.TabIndex = 0;
            // 
            // job_time_set_view
            // 
            this.AllowDrop = true;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1011, 716);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.ItemProperties);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.DataSource);
            this.Controls.Add(this.tabControl1);
            this.Name = "job_time_set_view";
            this.Text = "job_time_set_view";
            this.DragDrop += new System.Windows.Forms.DragEventHandler(this.job_time_set_view_on_drag_drop);
            this.DragEnter += new System.Windows.Forms.DragEventHandler(this.job_time_set_view_drag_enter);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.ListBox DataSource;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.ListBox ItemProperties;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.TabControl tabControl1;
    }
}

