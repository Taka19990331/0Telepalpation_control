function s=spectra(U,Y)
% Independent repetitions, not Welch time segments. Pairwise finite mask.
ok=isfinite(U)&isfinite(Y)&abs(U)>1e-12;
U(~ok)=NaN; Y(~ok)=NaN;
s.count=sum(ok,2);
s.Suu=mean(abs(U).^2,2,'omitnan'); s.Syy=mean(abs(Y).^2,2,'omitnan');
s.Suy=mean(conj(U).*Y,2,'omitnan');
s.H1=s.Suy./s.Suu;
s.Hmean=mean(Y./U,2,'omitnan');
s.coherence=abs(s.Suy).^2./(s.Suu.*s.Syy);
s.coherence(s.count<2 | s.Suu<=0 | s.Syy<=0)=NaN;
finite=isfinite(s.coherence);
s.coherence(finite)=min(1,max(0,s.coherence(finite))); % preserve unavailable NaNs
end
